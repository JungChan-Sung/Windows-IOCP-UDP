#include "UdpPacketDispatcherTests.h"

#include <WinSock2.h>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>

#include <Common/Packet/Game/GamePacket.h>
#include <Common/Packet/PacketSerialization.h>

#include <Server/Protocol/UdpPacketDispatcher.h>

#include <Tests/DebugTestResult.h>

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

	void WriteUInt16ToBuffer(
		common::packet::PacketBuffer& buffer,
		std::size_t offset,
		std::uint16_t value
	)
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

		const std::optional<common::packet::PacketBuffer> packetBuffer =
			common::packet::SerializePacket(packet);

		if (!packetBuffer.has_value())
		{
			return {};
		}

		return *packetBuffer;
	}

	void RunNullPacketDataTest(tests::DebugTestResult& result)
	{
		server::protocol::UdpPacketDispatcher dispatcher;
		const sockaddr_in remoteAddress = MakeRemoteAddress();

		const server::protocol::UdpPacketDispatcher::DispatchResult dispatchResult =
			dispatcher.Dispatch(remoteAddress, nullptr, 0);

		tests::Expect(
			result,
			dispatchResult.status
			== server::protocol::UdpPacketDispatcher::DispatchStatus::NullPacketData,
			"UdpPacketDispatcher: null packet data rejected"
		);

		tests::Expect(
			result,
			dispatchResult.actualPacketSize == 0,
			"UdpPacketDispatcher: null actual size"
		);
	}

	void RunPacketTooSmallTest(tests::DebugTestResult& result)
	{
		server::protocol::UdpPacketDispatcher dispatcher;
		const sockaddr_in remoteAddress = MakeRemoteAddress();
		const char packetData[2]{};

		const server::protocol::UdpPacketDispatcher::DispatchResult dispatchResult =
			dispatcher.Dispatch(
				remoteAddress,
				packetData,
				static_cast<int>(sizeof(packetData))
			);

		tests::Expect(
			result,
			dispatchResult.status
			== server::protocol::UdpPacketDispatcher::DispatchStatus::PacketTooSmall,
			"UdpPacketDispatcher: too small packet rejected"
		);

		tests::Expect(
			result,
			dispatchResult.actualPacketSize == static_cast<int>(sizeof(packetData)),
			"UdpPacketDispatcher: too small actual size"
		);
	}

	void RunInvalidHeaderSizeTest(tests::DebugTestResult& result)
	{
		server::protocol::UdpPacketDispatcher dispatcher;
		const sockaddr_in remoteAddress = MakeRemoteAddress();
		common::packet::PacketBuffer packetBuffer = MakeJoinRequestBuffer();

		WriteUInt16ToBuffer(
			packetBuffer,
			0,
			static_cast<std::uint16_t>(packetBuffer.size() + 1)
		);

		const server::protocol::UdpPacketDispatcher::DispatchResult dispatchResult =
			dispatcher.Dispatch(
				remoteAddress,
				packetBuffer.data(),
				static_cast<int>(packetBuffer.size())
			);

		tests::Expect(
			result,
			dispatchResult.status
			== server::protocol::UdpPacketDispatcher::DispatchStatus::InvalidHeaderSize,
			"UdpPacketDispatcher: invalid header size rejected"
		);

		tests::Expect(
			result,
			dispatchResult.declaredPacketSize == static_cast<int>(packetBuffer.size() + 1),
			"UdpPacketDispatcher: declared size captured"
		);
	}

	void RunUnsupportedProtocolVersionTest(tests::DebugTestResult& result)
	{
		server::protocol::UdpPacketDispatcher dispatcher;
		const sockaddr_in remoteAddress = MakeRemoteAddress();
		common::packet::PacketBuffer packetBuffer = MakeJoinRequestBuffer();

		WriteUInt16ToBuffer(
			packetBuffer,
			4,
			common::packet::protocolVersion + 1
		);

		const server::protocol::UdpPacketDispatcher::DispatchResult dispatchResult =
			dispatcher.Dispatch(
				remoteAddress,
				packetBuffer.data(),
				static_cast<int>(packetBuffer.size())
			);

		tests::Expect(
			result,
			dispatchResult.status
			== server::protocol::UdpPacketDispatcher::DispatchStatus::UnsupportedProtocolVersion,
			"UdpPacketDispatcher: unsupported protocol version rejected"
		);

		tests::Expect(
			result,
			dispatchResult.protocolVersion == common::packet::protocolVersion + 1,
			"UdpPacketDispatcher: protocol version captured"
		);
	}

	void RunUnknownPacketTypeTest(tests::DebugTestResult& result)
	{
		server::protocol::UdpPacketDispatcher dispatcher;
		const sockaddr_in remoteAddress = MakeRemoteAddress();
		common::packet::PacketBuffer packetBuffer = MakeJoinRequestBuffer();

		WriteUInt16ToBuffer(packetBuffer, 2, 999);

		const server::protocol::UdpPacketDispatcher::DispatchResult dispatchResult =
			dispatcher.Dispatch(
				remoteAddress,
				packetBuffer.data(),
				static_cast<int>(packetBuffer.size())
			);

		tests::Expect(
			result,
			dispatchResult.status
			== server::protocol::UdpPacketDispatcher::DispatchStatus::UnknownPacketType,
			"UdpPacketDispatcher: unknown packet type rejected"
		);

		tests::Expect(
			result,
			dispatchResult.packetType.has_value(),
			"UdpPacketDispatcher: unknown packet type captured"
		);
	}

	void RunInvalidPacketSizeTest(tests::DebugTestResult& result)
	{
		server::protocol::UdpPacketDispatcher dispatcher;
		const sockaddr_in remoteAddress = MakeRemoteAddress();
		common::packet::PacketBuffer packetBuffer = MakeJoinRequestBuffer();

		dispatcher.RegisterHandler(
			common::packet::PacketType::JoinRequest,
			common::packet::packetExpectedSize<common::packet::JoinRequestPacket> +1,
			[](const sockaddr_in&, const char*, int)
			{
				return server::protocol::UdpPacketDispatcher::PacketProcessResult{};
			}
		);

		const server::protocol::UdpPacketDispatcher::DispatchResult dispatchResult =
			dispatcher.Dispatch(
				remoteAddress,
				packetBuffer.data(),
				static_cast<int>(packetBuffer.size())
			);

		tests::Expect(
			result,
			dispatchResult.status
			== server::protocol::UdpPacketDispatcher::DispatchStatus::InvalidPacketSize,
			"UdpPacketDispatcher: invalid fixed packet size rejected"
		);

		tests::Expect(
			result,
			dispatchResult.expectedPacketSize
			== common::packet::packetExpectedSize<common::packet::JoinRequestPacket> +1,
			"UdpPacketDispatcher: expected packet size captured"
		);
	}

	void RunEmptyHandlerTest(tests::DebugTestResult& result)
	{
		server::protocol::UdpPacketDispatcher dispatcher;
		const sockaddr_in remoteAddress = MakeRemoteAddress();
		common::packet::PacketBuffer packetBuffer = MakeJoinRequestBuffer();

		dispatcher.RegisterHandler(
			common::packet::PacketType::JoinRequest,
			common::packet::packetExpectedSize<common::packet::JoinRequestPacket>,
			{}
		);

		const server::protocol::UdpPacketDispatcher::DispatchResult dispatchResult =
			dispatcher.Dispatch(
				remoteAddress,
				packetBuffer.data(),
				static_cast<int>(packetBuffer.size())
			);

		tests::Expect(
			result,
			dispatchResult.status
			== server::protocol::UdpPacketDispatcher::DispatchStatus::EmptyHandler,
			"UdpPacketDispatcher: empty handler rejected"
		);
	}

	void RunSucceededHandlerTest(tests::DebugTestResult& result)
	{
		server::protocol::UdpPacketDispatcher dispatcher;
		const sockaddr_in remoteAddress = MakeRemoteAddress();
		common::packet::PacketBuffer packetBuffer = MakeJoinRequestBuffer();

		bool handlerCalled = false;
		int handlerPacketSize = 0;

		dispatcher.RegisterHandler(
			common::packet::PacketType::JoinRequest,
			common::packet::packetExpectedSize<common::packet::JoinRequestPacket>,
			[&handlerCalled, &handlerPacketSize](
				const sockaddr_in&,
				const char*,
				int packetSize
				)
			{
				handlerCalled = true;
				handlerPacketSize = packetSize;

				server::protocol::UdpPacketDispatcher::PacketProcessResult processResult{};
				processResult.status =
					server::protocol::UdpPacketDispatcher::DispatchStatus::Succeeded;
				processResult.detailCode = 123;
				return processResult;
			}
		);

		const server::protocol::UdpPacketDispatcher::DispatchResult dispatchResult =
			dispatcher.Dispatch(
				remoteAddress,
				packetBuffer.data(),
				static_cast<int>(packetBuffer.size())
			);

		tests::Expect(
			result,
			dispatchResult.status
			== server::protocol::UdpPacketDispatcher::DispatchStatus::Succeeded,
			"UdpPacketDispatcher: handler succeeds"
		);

		tests::Expect(
			result,
			dispatchResult.detailCode == 123,
			"UdpPacketDispatcher: handler detail code propagated"
		);

		tests::Expect(
			result,
			handlerCalled,
			"UdpPacketDispatcher: handler called"
		);

		tests::Expect(
			result,
			handlerPacketSize == static_cast<int>(packetBuffer.size()),
			"UdpPacketDispatcher: handler packet size"
		);
	}

	void RunClearRemovesHandlersTest(tests::DebugTestResult& result)
	{
		server::protocol::UdpPacketDispatcher dispatcher;
		const sockaddr_in remoteAddress = MakeRemoteAddress();
		common::packet::PacketBuffer packetBuffer = MakeJoinRequestBuffer();

		dispatcher.RegisterHandler(
			common::packet::PacketType::JoinRequest,
			common::packet::packetExpectedSize<common::packet::JoinRequestPacket>,
			[](const sockaddr_in&, const char*, int)
			{
				return server::protocol::UdpPacketDispatcher::PacketProcessResult{};
			}
		);

		dispatcher.Clear();

		const server::protocol::UdpPacketDispatcher::DispatchResult dispatchResult =
			dispatcher.Dispatch(
				remoteAddress,
				packetBuffer.data(),
				static_cast<int>(packetBuffer.size())
			);

		tests::Expect(
			result,
			dispatchResult.status
			== server::protocol::UdpPacketDispatcher::DispatchStatus::UnknownPacketType,
			"UdpPacketDispatcher: clear removes handler"
		);
	}

	void RunRegisterHandlerReplacesExistingHandlerTest(
		tests::DebugTestResult& result
	)
	{
		server::protocol::UdpPacketDispatcher dispatcher;
		const sockaddr_in remoteAddress = MakeRemoteAddress();
		common::packet::PacketBuffer packetBuffer = MakeJoinRequestBuffer();

		dispatcher.RegisterHandler(
			common::packet::PacketType::JoinRequest,
			common::packet::packetExpectedSize<common::packet::JoinRequestPacket>,
			[](const sockaddr_in&, const char*, int)
			{
				server::protocol::UdpPacketDispatcher::PacketProcessResult processResult{};
				processResult.status =
					server::protocol::UdpPacketDispatcher::DispatchStatus::InvalidPacketPayload;
				processResult.detailCode = 1;
				return processResult;
			}
		);

		dispatcher.RegisterHandler(
			common::packet::PacketType::JoinRequest,
			common::packet::packetExpectedSize<common::packet::JoinRequestPacket>,
			[](const sockaddr_in&, const char*, int)
			{
				server::protocol::UdpPacketDispatcher::PacketProcessResult processResult{};
				processResult.status =
					server::protocol::UdpPacketDispatcher::DispatchStatus::Succeeded;
				processResult.detailCode = 2;
				return processResult;
			}
		);

		const server::protocol::UdpPacketDispatcher::DispatchResult dispatchResult =
			dispatcher.Dispatch(
				remoteAddress,
				packetBuffer.data(),
				static_cast<int>(packetBuffer.size())
			);

		tests::Expect(
			result,
			dispatchResult.status
			== server::protocol::UdpPacketDispatcher::DispatchStatus::Succeeded,
			"UdpPacketDispatcher: replacement handler succeeds"
		);

		tests::Expect(
			result,
			dispatchResult.detailCode == 2,
			"UdpPacketDispatcher: replacement handler detail code"
		);
	}

	void RunToStringTest(tests::DebugTestResult& result)
	{
		using DispatchStatus =
			server::protocol::UdpPacketDispatcher::DispatchStatus;

		tests::Expect(
			result,
			std::string_view(
				server::protocol::UdpPacketDispatcher::ToString(DispatchStatus::Succeeded)
			) == "Succeeded",
			"UdpPacketDispatcher: ToString Succeeded"
		);

		tests::Expect(
			result,
			std::string_view(
				server::protocol::UdpPacketDispatcher::ToString(DispatchStatus::NullPacketData)
			) == "NullPacketData",
			"UdpPacketDispatcher: ToString NullPacketData"
		);

		tests::Expect(
			result,
			std::string_view(
				server::protocol::UdpPacketDispatcher::ToString(DispatchStatus::PacketTooSmall)
			) == "PacketTooSmall",
			"UdpPacketDispatcher: ToString PacketTooSmall"
		);

		tests::Expect(
			result,
			std::string_view(
				server::protocol::UdpPacketDispatcher::ToString(DispatchStatus::InvalidPacketHeader)
			) == "InvalidPacketHeader",
			"UdpPacketDispatcher: ToString InvalidPacketHeader"
		);

		tests::Expect(
			result,
			std::string_view(
				server::protocol::UdpPacketDispatcher::ToString(DispatchStatus::InvalidHeaderSize)
			) == "InvalidHeaderSize",
			"UdpPacketDispatcher: ToString InvalidHeaderSize"
		);

		tests::Expect(
			result,
			std::string_view(
				server::protocol::UdpPacketDispatcher::ToString(DispatchStatus::UnsupportedProtocolVersion)
			) == "UnsupportedProtocolVersion",
			"UdpPacketDispatcher: ToString UnsupportedProtocolVersion"
		);

		tests::Expect(
			result,
			std::string_view(
				server::protocol::UdpPacketDispatcher::ToString(DispatchStatus::UnknownPacketType)
			) == "UnknownPacketType",
			"UdpPacketDispatcher: ToString UnknownPacketType"
		);

		tests::Expect(
			result,
			std::string_view(
				server::protocol::UdpPacketDispatcher::ToString(DispatchStatus::InvalidPacketSize)
			) == "InvalidPacketSize",
			"UdpPacketDispatcher: ToString InvalidPacketSize"
		);

		tests::Expect(
			result,
			std::string_view(
				server::protocol::UdpPacketDispatcher::ToString(DispatchStatus::InvalidPacketPayload)
			) == "InvalidPacketPayload",
			"UdpPacketDispatcher: ToString InvalidPacketPayload"
		);

		tests::Expect(
			result,
			std::string_view(
				server::protocol::UdpPacketDispatcher::ToString(DispatchStatus::EmptyHandler)
			) == "EmptyHandler",
			"UdpPacketDispatcher: ToString EmptyHandler"
		);

		tests::Expect(
			result,
			std::string_view(
				server::protocol::UdpPacketDispatcher::ToString(DispatchStatus::Count)
			) == "Unknown",
			"UdpPacketDispatcher: ToString Count"
		);
	}

	void RunInvalidPayloadResultPropagatedTest(
		tests::DebugTestResult& result
	)
	{
		server::protocol::UdpPacketDispatcher dispatcher;
		const sockaddr_in remoteAddress = MakeRemoteAddress();
		common::packet::PacketBuffer packetBuffer = MakeJoinRequestBuffer();

		dispatcher.RegisterHandler(
			common::packet::PacketType::JoinRequest,
			common::packet::packetExpectedSize<common::packet::JoinRequestPacket>,
			[](const sockaddr_in&, const char*, int)
			{
				server::protocol::UdpPacketDispatcher::PacketProcessResult processResult{};
				processResult.status =
					server::protocol::UdpPacketDispatcher::DispatchStatus::InvalidPacketPayload;
				processResult.detailCode = 77;
				return processResult;
			}
		);

		const server::protocol::UdpPacketDispatcher::DispatchResult dispatchResult =
			dispatcher.Dispatch(
				remoteAddress,
				packetBuffer.data(),
				static_cast<int>(packetBuffer.size())
			);

		tests::Expect(
			result,
			dispatchResult.status
			== server::protocol::UdpPacketDispatcher::DispatchStatus::InvalidPacketPayload,
			"UdpPacketDispatcher: invalid payload status propagated"
		);

		tests::Expect(
			result,
			dispatchResult.detailCode == 77,
			"UdpPacketDispatcher: invalid payload detail propagated"
		);
	}

	void RunVariableSizePacketHandlerTest(tests::DebugTestResult& result)
	{
		server::protocol::UdpPacketDispatcher dispatcher;
		const sockaddr_in remoteAddress = MakeRemoteAddress();

		common::packet::PlayerSnapshotPacket packet{};
		packet.serverTick = 10;
		packet.roomId = 1;
		packet.lastProcessedInputSequence = 5;
		packet.playerCount = 1;
		packet.players[0].playerId = 100;
		packet.players[0].x = 10.0F;
		packet.players[0].y = 20.0F;

		const std::optional<common::packet::PacketBuffer> packetBuffer =
			common::packet::SerializePacket(packet);

		tests::Expect(
			result,
			packetBuffer.has_value(),
			"UdpPacketDispatcher: variable packet serialize"
		);

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
				return server::protocol::UdpPacketDispatcher::PacketProcessResult{};
			}
		);

		const server::protocol::UdpPacketDispatcher::DispatchResult dispatchResult =
			dispatcher.Dispatch(
				remoteAddress,
				packetBuffer->data(),
				static_cast<int>(packetBuffer->size())
			);

		tests::Expect(
			result,
			common::packet::packetExpectedSize<common::packet::PlayerSnapshotPacket> == 0,
			"UdpPacketDispatcher: variable packet expected size is zero"
		);

		tests::Expect(
			result,
			dispatchResult.status
			== server::protocol::UdpPacketDispatcher::DispatchStatus::Succeeded,
			"UdpPacketDispatcher: variable packet handler succeeds"
		);

		tests::Expect(
			result,
			handlerCalled,
			"UdpPacketDispatcher: variable packet handler called"
		);
	}
}

namespace tests::server
{
	tests::DebugTestResult RunUdpPacketDispatcherTests()
	{
		tests::DebugTestResult result{};

		RunNullPacketDataTest(result);
		RunPacketTooSmallTest(result);
		RunInvalidHeaderSizeTest(result);
		RunUnsupportedProtocolVersionTest(result);
		RunUnknownPacketTypeTest(result);
		RunInvalidPacketSizeTest(result);
		RunEmptyHandlerTest(result);
		RunSucceededHandlerTest(result);
		RunClearRemovesHandlersTest(result);
		RunRegisterHandlerReplacesExistingHandlerTest(result);
		RunToStringTest(result);
		RunInvalidPayloadResultPropagatedTest(result);
		RunVariableSizePacketHandlerTest(result);

		return result;
	}
}