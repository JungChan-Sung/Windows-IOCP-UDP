#include "AuthenticatedUdpPacket.h"

#include <cstring>
#include <limits>

#include <Common/Net/Auth/PacketAuthenticator.h>
#include <Common/Packet/Serialization/PacketReader.h>
#include <Common/Packet/Serialization/PacketSerializationCore.h>
#include <Common/Packet/Serialization/PacketWriter.h>

namespace common::net
{
	std::optional<packet::PacketBuffer> BuildAuthenticatedUdpPacket(
		const SessionToken& sessionToken,
		PacketAuthenticationSequence sequence,
		packet::ConstPacketSpan serializedPacket
	)
	{
		const std::optional<packet::PacketHeader> packetHeader = packet::DeserializePacketHeader(
			serializedPacket.data(),
			static_cast<int>(serializedPacket.size())
		);
		if (!packetHeader.has_value())
		{
			return std::nullopt;
		}

		if (packetHeader->size != serializedPacket.size())
		{
			return std::nullopt;
		}

		if (packet::GetPacketHeaderProtocolVersion(*packetHeader) != packet::protocolVersion)
		{
			return std::nullopt;
		}

		if (packet::IsAuthenticatedPacketHeader(*packetHeader))
		{
			return std::nullopt;
		}

		const std::size_t authenticatedPacketSize = serializedPacket.size() + authenticatedUdpPacketTrailerWireSize;
		if (authenticatedPacketSize > packet::maxSerializedPacketSize || authenticatedPacketSize > std::numeric_limits<std::uint16_t>::max())
		{
			return std::nullopt;
		}

		packet::PacketWriter headerWriter;
		headerWriter.Reserve(authenticatedPacketSize);

		packet::WritePacketHeader(
			headerWriter,
			static_cast<std::uint16_t>(authenticatedPacketSize),
			packetHeader->type,
			packet::IsReliablePacketHeader(*packetHeader),
			true
		);

		packet::PacketBuffer packetBuffer = headerWriter.TakeBuffer();
		const packet::ConstPacketSpan packetBody = serializedPacket.subspan(packet::serializedPacketHeaderSize);
		packetBuffer.insert(packetBuffer.end(), packetBody.begin(), packetBody.end());

		packet::PacketWriter sequenceWriter;
		sequenceWriter.WriteUInt32(sequence);

		packet::PacketBuffer sequenceBuffer = sequenceWriter.TakeBuffer();
		packetBuffer.insert(packetBuffer.end(), sequenceBuffer.begin(), sequenceBuffer.end());

		PacketAuthenticationTag tag{};

		if (!ComputePacketAuthenticationTag(sessionToken, std::span<const char>(packetBuffer.data(), packetBuffer.size()), tag))
		{
			return std::nullopt;
		}

		for (const std::uint8_t value : tag)
		{
			packetBuffer.push_back(static_cast<char>(value));
		}

		if (packetBuffer.size() != authenticatedPacketSize)
		{
			return std::nullopt;
		}

		return packetBuffer;
	}

	std::optional<AuthenticatedUdpPacketView> ParseAuthenticatedUdpPacket(const char* packetData, int packetSize) noexcept
	{
		constexpr std::size_t minimumPacketSize = packet::serializedPacketHeaderSize + authenticatedUdpPacketTrailerWireSize;
		if (packetData == nullptr || packetSize < static_cast<int>(minimumPacketSize))
		{
			return std::nullopt;
		}

		const std::optional<packet::PacketHeader> packetHeader = packet::DeserializePacketHeader(packetData, packetSize);
		if (!packetHeader.has_value())
		{
			return std::nullopt;
		}

		if (packetHeader->size != packetSize)
		{
			return std::nullopt;
		}

		if (packet::GetPacketHeaderProtocolVersion(*packetHeader) != packet::protocolVersion)
		{
			return std::nullopt;
		}

		if (!packet::IsAuthenticatedPacketHeader(*packetHeader))
		{
			return std::nullopt;
		}

		const std::size_t sequenceOffset = static_cast<std::size_t>(packetSize) - packetAuthenticationTagSize - packetAuthenticationSequenceWireSize;
		packet::PacketReader sequenceReader(packetData + sequenceOffset, static_cast<int>(packetAuthenticationSequenceWireSize));

		PacketAuthentication authentication{};
		if (!sequenceReader.ReadUInt32(authentication.sequence) || !sequenceReader.IsComplete())
		{
			return std::nullopt;
		}

		const std::size_t tagOffset = static_cast<std::size_t>(packetSize) - packetAuthenticationTagSize;
		std::memcpy(authentication.tag.data(), packetData + tagOffset, authentication.tag.size());

		return AuthenticatedUdpPacketView{
			.packetHeader = *packetHeader,
			.authentication = authentication,
			.packetData = packet::ConstPacketSpan(packetData, static_cast<std::size_t>(packetSize)),
		};
	}

	bool VerifyAuthenticatedUdpPacket(const SessionToken& sessionToken, const AuthenticatedUdpPacketView& packetView) noexcept
	{
		if (packetView.packetData.size() < authenticatedUdpPacketTrailerWireSize)
		{
			return false;
		}

		const std::size_t authenticatedDataSize = packetView.packetData.size() - packetAuthenticationTagSize;
		return VerifyPacketAuthenticationTag(sessionToken, packetView.packetData.first(authenticatedDataSize), packetView.authentication.tag);
	}

	std::optional<packet::PacketBuffer> BuildUnauthenticatedUdpPacket(const AuthenticatedUdpPacketView& packetView)
	{
		if (!packet::IsAuthenticatedPacketHeader(packetView.packetHeader))
		{
			return std::nullopt;
		}

		if (packetView.packetData.size() != packetView.packetHeader.size)
		{
			return std::nullopt;
		}

		if (packetView.packetData.size() < packet::serializedPacketHeaderSize + authenticatedUdpPacketTrailerWireSize)
		{
			return std::nullopt;
		}

		const std::size_t packetSize = packetView.packetData.size() - authenticatedUdpPacketTrailerWireSize;
		packet::PacketWriter writer;
		writer.Reserve(packetSize);

		packet::WritePacketHeader(
			writer,
			static_cast<std::uint16_t>(packetSize),
			packetView.packetHeader.type,
			packet::IsReliablePacketHeader(packetView.packetHeader)
		);

		packet::PacketBuffer packetBuffer = writer.TakeBuffer();
		const auto bodyBegin = packetView.packetData.begin() + packet::serializedPacketHeaderSize;
		const auto bodyEnd = packetView.packetData.begin() + packetSize;
		packetBuffer.insert( packetBuffer.end(), bodyBegin, bodyEnd);
		if (packetBuffer.size() != packetSize)
		{
			return std::nullopt;
		}

		return packetBuffer;
	}
}