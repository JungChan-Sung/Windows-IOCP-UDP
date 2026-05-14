#include "UdpPacketDispatcherTests.h"

#include <WinSock2.h>

#include <cstddef>
#include <cstdint>
#include <optional>

#include <Common/Diagnostics/DebugTestResult.h>
#include <Common/Packet/GamePacket.h>
#include <Common/Packet/PacketSerialization.h>

#include <Server/Net/UdpPacketDispatcher.h>

namespace
{
	[[nodiscard]] sockaddr_in MakeRemoteAddress() noexcept
	{
		sockaddr_in remoteAddress{};
		remoteAddress.sin_family = AF_INET;
		remoteAddress.sin_addr.S_un.S_addr = ::htonl(0x7F000001);
		remoteAddress.sin_port = ::htons(9000);
		return remoteAddress;
	}

	void WriteUInt16ToBuffer(common::packet::PacketBuffer& buffer, std::size_t offset, std::uint16_t value)
	{
		if (offset + 1 >= buffer.size())
		{
			return;
		}

		buffer[offset] = static_cast<char>(value & 0x00FF);
		buffer[offset + 1] = static_cast<char>((value >> 8) & 0x00FF);
	}

	[[nodiscard]] common::packet::PacketBuffer MakeJoinRequestBuffer()
	{
		common::packet::JoinRequestPacket packet{};

		const std::optional<common::packet::PacketBuffer> packetBuffer = common::packet::SerializePacket(packet);
		if (!packetBuffer.has_value())
		{
			return {};
		}

		return *packetBuffer;
	}

	void RunNullPacketDataTest(common::diagnostics::DebugTestResult& result)
	{
		server::net::UdpPacketDispatcher dispatcher;
		const sockaddr_in remoteAddress = MakeRemoteAddress();

		const server::net::UdpPacketDispatcher::DispatchResult dispatchResult = dispatcher.Dispatch(remoteAddress, nullptr, 0);

		common::diagnostics::Expect(result, dispatchResult.status == server::net::UdpPacketDispatcher::DispatchStatus::NullPacketData,
			"UdpPacketDispatcher: null packet data rejected");
		common::diagnostics::Expect(result, dispatchResult.actualPacketSize == 0, "UdpPacketDispatcher: null actual size");
	}

	void RunPacketTooSmallTest(common::diagnostics::DebugTestResult& result)
	{
		server::net::UdpPacketDispatcher dispatcher;
		const sockaddr_in remoteAddress = MakeRemoteAddress();
		const char packetData[2]{};

		const server::net::UdpPacketDispatcher::DispatchResult dispatchResult = dispatcher.Dispatch(
			remoteAddress,
			packetData,
			static_cast<int>(sizeof(packetData))
		);

		common::diagnostics::Expect(result, dispatchResult.status == server::net::UdpPacketDispatcher::DispatchStatus::PacketTooSmall,
			"UdpPacketDispatcher: too small packet rejected");
		common::diagnostics::Expect(result, dispatchResult.actualPacketSize == static_cast<int>(sizeof(packetData)),
			"UdpPacketDispatcher: too small actual size");
	}

	void RunInvalidHeaderSizeTest(common::diagnostics::DebugTestResult& result)
	{
		server::net::UdpPacketDispatcher dispatcher;
		const sockaddr_in remoteAddress = MakeRemoteAddress();
		common::packet::PacketBuffer packetBuffer = MakeJoinRequestBuffer();

		WriteUInt16ToBuffer(packetBuffer, 0, static_cast<std::uint16_t>(packetBuffer.size() + 1));

		const server::net::UdpPacketDispatcher::DispatchResult dispatchResult = dispatcher.Dispatch(
			remoteAddress,
			packetBuffer.data(),
			static_cast<int>(packetBuffer.size())
		);

		common::diagnostics::Expect(result, dispatchResult.status == server::net::UdpPacketDispatcher::DispatchStatus::InvalidHeaderSize,
			"UdpPacketDispatcher: invalid header size rejected");
		common::diagnostics::Expect(result, dispatchResult.declaredPacketSize == static_cast<int>(packetBuffer.size() + 1),
			"UdpPacketDispatcher: declared size captured");
	}

	void RunUnsupportedProtocolVersionTest(common::diagnostics::DebugTestResult& result)
	{
		server::net::UdpPacketDispatcher dispatcher;
		const sockaddr_in remoteAddress = MakeRemoteAddress();
		common::packet::PacketBuffer packetBuffer = MakeJoinRequestBuffer();

		WriteUInt16ToBuffer(packetBuffer, 4, common::packet::protocolVersion + 1);

		const server::net::UdpPacketDispatcher::DispatchResult dispatchResult = dispatcher.Dispatch(
			remoteAddress,
			packetBuffer.data(),
			static_cast<int>(packetBuffer.size())
		);

		common::diagnostics::Expect(result, dispatchResult.status == server::net::UdpPacketDispatcher::DispatchStatus::UnsupportedProtocolVersion,
			"UdpPacketDispatcher: unsupported protocol version rejected");
		common::diagnostics::Expect(result, dispatchResult.protocolVersion == common::packet::protocolVersion + 1,
			"UdpPacketDispatcher: protocol version captured");
	}

	void RunUnknownPacketTypeTest(common::diagnostics::DebugTestResult& result)
	{
		server::net::UdpPacketDispatcher dispatcher;
		const sockaddr_in remoteAddress = MakeRemoteAddress();
		common::packet::PacketBuffer packetBuffer = MakeJoinRequestBuffer();

		WriteUInt16ToBuffer(packetBuffer, 2, 999);

		const server::net::UdpPacketDispatcher::DispatchResult dispatchResult = dispatcher.Dispatch(
			remoteAddress,
			packetBuffer.data(),
			static_cast<int>(packetBuffer.size())
		);

		common::diagnostics::Expect(result, dispatchResult.status == server::net::UdpPacketDispatcher::DispatchStatus::UnknownPacketType,
			"UdpPacketDispatcher: unknown packet type rejected");
		common::diagnostics::Expect(result, dispatchResult.packetType.has_value(), "UdpPacketDispatcher: unknown packet type captured");
	}

	void RunInvalidPacketSizeTest(common::diagnostics::DebugTestResult& result)
	{
		server::net::UdpPacketDispatcher dispatcher;
		const sockaddr_in remoteAddress = MakeRemoteAddress();
		common::packet::PacketBuffer packetBuffer = MakeJoinRequestBuffer();

		dispatcher.RegisterHandler(
			common::packet::PacketType::JoinRequest,
			common::packet::packetExpectedSize<common::packet::JoinRequestPacket> +1,
			[](const sockaddr_in&, const char*, int)
			{
				return server::net::UdpPacketDispatcher::PacketProcessResult{};
			}
		);

		const server::net::UdpPacketDispatcher::DispatchResult dispatchResult = dispatcher.Dispatch(
			remoteAddress,
			packetBuffer.data(),
			static_cast<int>(packetBuffer.size())
		);

		common::diagnostics::Expect(result, dispatchResult.status == server::net::UdpPacketDispatcher::DispatchStatus::InvalidPacketSize,
			"UdpPacketDispatcher: invalid fixed packet size rejected");
		common::diagnostics::Expect(result, dispatchResult.expectedPacketSize == common::packet::packetExpectedSize<common::packet::JoinRequestPacket> +1,
			"UdpPacketDispatcher: expected packet size captured");
	}

	void RunEmptyHandlerTest(common::diagnostics::DebugTestResult& result)
	{
		server::net::UdpPacketDispatcher dispatcher;
		const sockaddr_in remoteAddress = MakeRemoteAddress();
		common::packet::PacketBuffer packetBuffer = MakeJoinRequestBuffer();

		dispatcher.RegisterHandler(
			common::packet::PacketType::JoinRequest,
			common::packet::packetExpectedSize<common::packet::JoinRequestPacket>,
			{}
		);

		const server::net::UdpPacketDispatcher::DispatchResult dispatchResult = dispatcher.Dispatch(
			remoteAddress,
			packetBuffer.data(),
			static_cast<int>(packetBuffer.size())
		);

		common::diagnostics::Expect(result, dispatchResult.status == server::net::UdpPacketDispatcher::DispatchStatus::EmptyHandler,
			"UdpPacketDispatcher: empty handler rejected");
	}

	void RunSucceededHandlerTest(common::diagnostics::DebugTestResult& result)
	{
		server::net::UdpPacketDispatcher dispatcher;
		const sockaddr_in remoteAddress = MakeRemoteAddress();
		common::packet::PacketBuffer packetBuffer = MakeJoinRequestBuffer();

		bool handlerCalled = false;
		int handlerPacketSize = 0;

		dispatcher.RegisterHandler(
			common::packet::PacketType::JoinRequest,
			common::packet::packetExpectedSize<common::packet::JoinRequestPacket>,
			[&handlerCalled, &handlerPacketSize](const sockaddr_in&, const char*, int packetSize)
			{
				handlerCalled = true;
				handlerPacketSize = packetSize;

				server::net::UdpPacketDispatcher::PacketProcessResult processResult{};
				processResult.status = server::net::UdpPacketDispatcher::DispatchStatus::Succeeded;
				processResult.detailCode = 123;
				return processResult;
			}
		);

		const server::net::UdpPacketDispatcher::DispatchResult dispatchResult = dispatcher.Dispatch(
			remoteAddress,
			packetBuffer.data(),
			static_cast<int>(packetBuffer.size())
		);

		common::diagnostics::Expect(result, dispatchResult.status == server::net::UdpPacketDispatcher::DispatchStatus::Succeeded,
			"UdpPacketDispatcher: handler succeeds");
		common::diagnostics::Expect(result, dispatchResult.detailCode == 123, "UdpPacketDispatcher: handler detail code propagated");
		common::diagnostics::Expect(result, handlerCalled, "UdpPacketDispatcher: handler called");
		common::diagnostics::Expect(result, handlerPacketSize == static_cast<int>(packetBuffer.size()), "UdpPacketDispatcher: handler packet size");
	}

	void RunInvalidPayloadResultPropagatedTest(common::diagnostics::DebugTestResult& result)
	{
		server::net::UdpPacketDispatcher dispatcher;
		const sockaddr_in remoteAddress = MakeRemoteAddress();
		common::packet::PacketBuffer packetBuffer = MakeJoinRequestBuffer();

		dispatcher.RegisterHandler(
			common::packet::PacketType::JoinRequest,
			common::packet::packetExpectedSize<common::packet::JoinRequestPacket>,
			[](const sockaddr_in&, const char*, int)
			{
				server::net::UdpPacketDispatcher::PacketProcessResult processResult{};
				processResult.status = server::net::UdpPacketDispatcher::DispatchStatus::InvalidPacketPayload;
				processResult.detailCode = 77;
				return processResult;
			}
		);

		const server::net::UdpPacketDispatcher::DispatchResult dispatchResult = dispatcher.Dispatch(
			remoteAddress,
			packetBuffer.data(),
			static_cast<int>(packetBuffer.size())
		);

		common::diagnostics::Expect(result, dispatchResult.status == server::net::UdpPacketDispatcher::DispatchStatus::InvalidPacketPayload,
			"UdpPacketDispatcher: invalid payload status propagated");
		common::diagnostics::Expect(result, dispatchResult.detailCode == 77, "UdpPacketDispatcher: invalid payload detail propagated");
	}

	void RunVariableSizePacketHandlerTest(common::diagnostics::DebugTestResult& result)
	{
		server::net::UdpPacketDispatcher dispatcher;
		const sockaddr_in remoteAddress = MakeRemoteAddress();

		common::packet::PlayerSnapshotPacket packet{};
		packet.serverTick = 10;
		packet.roomId = 1;
		packet.lastProcessedInputSequence = 5;
		packet.playerCount = 1;
		packet.players[0].playerId = 100;
		packet.players[0].x = 10.0F;
		packet.players[0].y = 20.0F;

		const std::optional<common::packet::PacketBuffer> packetBuffer = common::packet::SerializePacket(packet);
		common::diagnostics::Expect(result, packetBuffer.has_value(), "UdpPacketDispatcher: variable packet serialize");

		if (!packetBuffer.has_value())
		{
			return;
		}

		bool handlerCalled = false;

		dispatcher.RegisterHandler(
			common::packet::PacketType::PlayerSnapshot,
			common::packet::packetExpectedSize<common::packet::PlayerSnapshotPacket>,
			[&handlerCalled](const sockaddr_in&, const char*, int)
			{
				handlerCalled = true;
				return server::net::UdpPacketDispatcher::PacketProcessResult{};
			}
		);

		const server::net::UdpPacketDispatcher::DispatchResult dispatchResult = dispatcher.Dispatch(
			remoteAddress,
			packetBuffer->data(),
			static_cast<int>(packetBuffer->size())
		);

		common::diagnostics::Expect(result, common::packet::packetExpectedSize<common::packet::PlayerSnapshotPacket> == 0,
			"UdpPacketDispatcher: variable packet expected size is zero");
		common::diagnostics::Expect(result, dispatchResult.status == server::net::UdpPacketDispatcher::DispatchStatus::Succeeded,
			"UdpPacketDispatcher: variable packet handler succeeds");
		common::diagnostics::Expect(result, handlerCalled, "UdpPacketDispatcher: variable packet handler called");
	}
}

namespace tests::server
{
	common::diagnostics::DebugTestResult RunUdpPacketDispatcherTests()
	{
		common::diagnostics::DebugTestResult result{};

		RunNullPacketDataTest(result);
		RunPacketTooSmallTest(result);
		RunInvalidHeaderSizeTest(result);
		RunUnsupportedProtocolVersionTest(result);
		RunUnknownPacketTypeTest(result);
		RunInvalidPacketSizeTest(result);
		RunEmptyHandlerTest(result);
		RunSucceededHandlerTest(result);
		RunInvalidPayloadResultPropagatedTest(result);
		RunVariableSizePacketHandlerTest(result);

		return result;
	}
}