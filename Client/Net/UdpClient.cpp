#include "UdpClient.h"

#include <expected>
#include <functional>
#include <optional>
#include <string>
#include <type_traits>
#include <variant>

#include <Common/Net/ReliableUdpSession.h>
#include <Common/Packet/GamePacket.h>
#include <Common/Packet/PacketSerialization.h>
#include <Common/Packet/PacketReliability.h>
#include <Common/Packet/ReliableUdpPacketBuilder.h>
#include <Common/String/StringFormat.h>

#include <Client/Game/ClientWorld.h>

namespace
{
	template <typename TObject, typename TPacket>
	using PacketMemberHandler = void (TObject::*)(const TPacket&);

	template <typename TPacket, typename TObject>
	void RegisterTypedPacketHandler(
		client::net::ClientPacketDispatcher& packetDispatcher,
		common::packet::PacketType packetType,
		TObject& object,
		PacketMemberHandler<TObject, TPacket> handler
	)
	{
		using Packet = std::remove_cvref_t<TPacket>;

		packetDispatcher.RegisterHandler(
			packetType,
			common::packet::packetExpectedSize<Packet>,
			[&object, handler](const char* packetData, int packetSize)
			{
				std::optional<Packet> packet = common::packet::DeserializePacket<Packet>(packetData, packetSize);
				if (!packet.has_value())
				{
					return;
				}

				std::invoke(handler, object, *packet);
			}
		);
	}
}

namespace client::net
{
	UdpClient::~UdpClient() noexcept
	{
		Stop();
	}

	std::string UdpClient::ToString(const StartError& startError)
	{
		return std::visit(
			[](const auto& error) -> std::string
			{
				using ErrorType = std::remove_cvref_t<decltype(error)>;

				if constexpr (std::is_same_v<ErrorType, StartFailure>)
				{
					switch (error)
					{
					case StartFailure::AlreadyRunning:
						return "AlreadyRunning";

					case StartFailure::InvalidTransportType:
						return "InvalidTransportType";

					default:
						return "Unknown";
					}
				}
				else if constexpr (std::is_same_v<ErrorType, UdpSocketTransport::StartError>)
				{
					return common::string::FormatScopedName("SocketTransport", UdpSocketTransport::ToString(error));
				}
				else if constexpr (std::is_same_v<ErrorType, UdpIocpTransport::StartError>)
				{
					return common::string::FormatScopedName("IocpTransport", UdpIocpTransport::ToString(error));
				}
				else
				{
					return "Unknown";
				}
			},
			startError
		);
	}

	UdpClient::StartResult UdpClient::Start(const char* serverIp, unsigned short serverPort, ClientWorldType& world)
	{
		if (isRunning_.load())
		{
			return std::unexpected(StartError{ StartFailure::AlreadyRunning });
		}

		world_ = &world;
		inputSequence_ = 0;

		{
			std::scoped_lock lock(reliableSessionMutex_);
			reliableSession_.Reset();
		}

		packetDispatcher_.Clear();
		snapshotChunkAssembler_.Clear();
		snapshotChunkAssembler_.SetAssemblyTimeout(snapshotAssemblyTimeout_);
		RegisterPacketHandlers();

		const StartResult startTransportResult = StartTransport(serverIp, serverPort);
		if (!startTransportResult.has_value())
		{
			world_ = nullptr;
			packetDispatcher_.Clear();
			snapshotChunkAssembler_.Clear();
			return startTransportResult;
		}

		isRunning_.store(true);
		return {};
	}

	void UdpClient::Stop() noexcept
	{
		if (!isRunning_.exchange(false))
		{
			return;
		}

		StopTransport();

		world_ = nullptr;
		inputSequence_ = 0;

		{
			std::scoped_lock lock(reliableSessionMutex_);
			reliableSession_.Reset();
		}

		packetDispatcher_.Clear();
		snapshotChunkAssembler_.Clear();
	}

	bool UdpClient::SendJoinRequest()
	{
		common::packet::JoinRequestPacket packet{};

		const std::optional<common::packet::PacketBuffer> packetBuffer = common::packet::SerializePacket(packet);
		if (!packetBuffer.has_value())
		{
			return false;
		}

		return SendSerializedGamePacket(common::packet::ConstPacketSpan(packetBuffer->data(), (packetBuffer->size())));
	}

	bool UdpClient::SendInputCommand(common::game::InputFlags inputFlags, std::uint32_t& inputSequence)
	{
		common::packet::InputCommandPacket packet{};
		packet.inputSequence = ++inputSequence_;
		packet.inputFlags = inputFlags;

		const std::optional<common::packet::PacketBuffer> packetBuffer = common::packet::SerializePacket(packet);
		if (!packetBuffer.has_value())
		{
			return false;
		}

		inputSequence = packet.inputSequence;
		return SendSerializedGamePacket(common::packet::ConstPacketSpan(packetBuffer->data(), (packetBuffer->size())));
	}

	bool UdpClient::SendFireRequest()
	{
		common::packet::FireRequestPacket packet{};

		const std::optional<common::packet::PacketBuffer> packetBuffer = common::packet::SerializePacket(packet);
		if (!packetBuffer.has_value())
		{
			return false;
		}

		return SendSerializedGamePacket(common::packet::ConstPacketSpan(packetBuffer->data(), (packetBuffer->size())));
	}

	bool UdpClient::SendLeaveRequest()
	{
		common::packet::LeaveRequestPacket packet{};

		const std::optional<common::packet::PacketBuffer> packetBuffer = common::packet::SerializePacket(packet);
		if (!packetBuffer.has_value())
		{
			return false;
		}

		return SendSerializedGamePacket(common::packet::ConstPacketSpan(packetBuffer->data(), packetBuffer->size()));
	}

	bool UdpClient::SendJoinRoomRequest(RoomId roomId)
	{
		common::packet::JoinRoomRequestPacket packet{};
		packet.roomId = roomId;

		const std::optional<common::packet::PacketBuffer> packetBuffer = common::packet::SerializePacket(packet);
		if (!packetBuffer.has_value())
		{
			return false;
		}

		return SendSerializedGamePacket(common::packet::ConstPacketSpan(packetBuffer->data(), packetBuffer->size()));
	}

	void UdpClient::ProcessReliableResends()
	{
		const common::net::ReliableUdpSession::TimePoint currentTime = common::net::ReliableUdpSession::Clock::now();
		common::net::ReliableUdpSession::ResendPacketList resendPacketList;

		{
			std::scoped_lock lock(reliableSessionMutex_);
			resendPacketList = reliableSession_.ExtractResendPackets(currentTime);
		}

		for (const common::net::ReliablePendingPacket& pendingPacket : resendPacketList)
		{
			SendPacket(pendingPacket.packetBuffer.data(), static_cast<int>(pendingPacket.packetBuffer.size()));
		}
	}

	UdpClient::StartResult UdpClient::StartTransport(const char* serverIp, unsigned short serverPort)
	{
		switch (transportType_)
		{
		case config::ClientTransportType::Socket:
		{
			const UdpSocketTransport::StartResult startResult = socketTransport_.Start(
				serverIp,
				serverPort,
				[this](const char* packetData, int packetSize)
				{
					HandlePacket(packetData, packetSize);
				}
			);
			if (!startResult.has_value())
			{
				return std::unexpected(StartError{ startResult.error() });
			}

			return {};
		}

		case config::ClientTransportType::Iocp:
		{
			const UdpIocpTransport::StartResult startResult = iocpTransport_.Start(
				serverIp,
				serverPort,
				iocpWorkerThreadCount_,
				iocpRecvContextCount_,
				[this](const char* packetData, int packetSize)
				{
					HandlePacket(packetData, packetSize);
				}
			);
			if (!startResult.has_value())
			{
				return std::unexpected(StartError{ startResult.error() });
			}

			return {};
		}

		default:
			return std::unexpected(StartError{ StartFailure::InvalidTransportType });
		}
	}

	void UdpClient::StopTransport() noexcept
	{
		socketTransport_.Stop();
		iocpTransport_.Stop();
	}

	bool UdpClient::SendPacket(const void* packetData, int packetSize)
	{
		switch (transportType_)
		{
		case config::ClientTransportType::Socket:
			return socketTransport_.SendPacket(packetData, packetSize);

		case config::ClientTransportType::Iocp:
			return iocpTransport_.SendPacket(packetData, packetSize);

		default:
			return false;
		}
	}

	bool UdpClient::SendSerializedGamePacket(common::packet::ConstPacketSpan serializedGamePacket)
	{
		const std::optional<common::packet::PacketHeader> packetHeader =
			common::packet::DeserializePacketHeader(
				serializedGamePacket.data(),
				static_cast<int>(serializedGamePacket.size())
			);

		if (!packetHeader.has_value())
		{
			return false;
		}

		if (common::packet::IsReliablePacketHeader(*packetHeader))
		{
			return false;
		}

		if (common::packet::GetPacketHeaderProtocolVersion(*packetHeader) != common::packet::protocolVersion)
		{
			return false;
		}

		if (static_cast<std::size_t>(packetHeader->size) != serializedGamePacket.size())
		{
			return false;
		}

		if (common::packet::IsReliablePacketType(packetHeader->type))
		{
			return SendReliablePacket(serializedGamePacket);
		}

		return SendPacket(serializedGamePacket.data(), static_cast<int>(serializedGamePacket.size()));
	}

	bool UdpClient::SendReliablePacket(common::packet::ConstPacketSpan serializedGamePacket)
	{
		std::optional<common::packet::PacketBuffer> reliablePacketBuffer;

		{
			std::scoped_lock lock(reliableSessionMutex_);

			const common::net::ReliableSequence sequence = reliableSession_.AllocateOutgoingSequence();
			const common::net::ReliableUdpPacketHeader reliableHeader = reliableSession_.BuildOutgoingHeader(sequence);

			reliablePacketBuffer = common::packet::BuildReliableUdpPacket(reliableHeader, serializedGamePacket);
			if (!reliablePacketBuffer.has_value())
			{
				return false;
			}

			const common::net::ReliableUdpSession::TimePoint currentTime = common::net::ReliableUdpSession::Clock::now();

			if (!reliableSession_.RegisterSentPacket(sequence, *reliablePacketBuffer, currentTime))
			{
				return false;
			}
		}

		return SendPacket(reliablePacketBuffer->data(), static_cast<int>(reliablePacketBuffer->size()));
	}

	bool UdpClient::SendReliableAckPacket()
	{
		common::net::ReliableUdpPacketHeader reliableHeader{};

		{
			std::scoped_lock lock(reliableSessionMutex_);
			reliableHeader = reliableSession_.BuildOutgoingAckHeader();
		}

		const std::optional<common::packet::PacketBuffer> ackPacketBuffer = common::packet::BuildReliableUdpAckPacket(reliableHeader);
		if (!ackPacketBuffer.has_value())
		{
			return false;
		}

		return SendPacket(ackPacketBuffer->data(), static_cast<int>(ackPacketBuffer->size()));
	}

	void UdpClient::RegisterPacketHandlers()
	{
		packetDispatcher_.Clear();

		RegisterTypedPacketHandler(
			packetDispatcher_,
			common::packet::PacketType::JoinResponse,
			*this,
			&UdpClient::HandleJoinResponse
		);

		RegisterTypedPacketHandler(
			packetDispatcher_,
			common::packet::PacketType::JoinRoomResponse,
			*this,
			&UdpClient::HandleJoinRoomResponse
		);

		RegisterTypedPacketHandler(
			packetDispatcher_,
			common::packet::PacketType::PlayerJoined,
			*this,
			&UdpClient::HandlePlayerJoined
		);

		RegisterTypedPacketHandler(
			packetDispatcher_,
			common::packet::PacketType::PlayerLeft,
			*this,
			&UdpClient::HandlePlayerLeft
		);

		RegisterTypedPacketHandler(
			packetDispatcher_,
			common::packet::PacketType::PlayerSnapshot,
			*this,
			&UdpClient::HandlePlayerSnapshot
		);

		RegisterTypedPacketHandler(
			packetDispatcher_,
			common::packet::PacketType::BulletSnapshot,
			*this,
			&UdpClient::HandleBulletSnapshot
		);

		RegisterTypedPacketHandler(
			packetDispatcher_,
			common::packet::PacketType::ImpactEffect,
			*this,
			&UdpClient::HandleImpactEffectPacket
		);
	}

	void UdpClient::HandlePacket(const char* packetData, int packetSize)
	{
		const std::optional<common::packet::PacketHeader> packetHeader = common::packet::DeserializePacketHeader(packetData, packetSize);

		if (packetHeader.has_value() && common::packet::IsReliablePacketHeader(*packetHeader))
		{
			HandleReliablePacket(packetData, packetSize);
			return;
		}

		packetDispatcher_.Dispatch(packetData, packetSize);
	}

	void UdpClient::HandleReliablePacket(const char* packetData, int packetSize)
	{
		const std::optional<common::packet::ReliableUdpPacketView> packetView = common::packet::ParseReliableUdpPacket(packetData, packetSize);
		if (!packetView.has_value())
		{
			return;
		}

		const bool isAckOnlyPacket = packetView->packetHeader.type == common::packet::PacketType::None;
		if (isAckOnlyPacket)
		{
			std::scoped_lock lock(reliableSessionMutex_);
			static_cast<void>(reliableSession_.ProcessReceivedAck(packetView->reliableHeader));
			return;
		}

		bool isNewReliablePacket = false;

		{
			std::scoped_lock lock(reliableSessionMutex_);

			isNewReliablePacket = reliableSession_.ProcessReceivedDataHeader(packetView->reliableHeader);
		}

		static_cast<void>(SendReliableAckPacket());

		if (!isNewReliablePacket)
		{
			return;
		}

		const std::optional<common::packet::PacketBuffer> gamePacketBuffer = common::packet::BuildGamePacketFromReliableUdpPacketView(*packetView);
		if (!gamePacketBuffer.has_value())
		{
			return;
		}

		packetDispatcher_.Dispatch(gamePacketBuffer->data(), static_cast<int>(gamePacketBuffer->size()));
	}

	void UdpClient::HandleJoinResponse(const common::packet::JoinResponsePacket& packet)
	{
		if (world_ == nullptr)
		{
			return;
		}

		if (!world_->TrySetJoinState(packet.playerId, packet.roomId, packet.spawnX, packet.spawnY))
		{
			return;
		}

		game::ClientWorld::PlayerJoinedEvent playerJoinedEvent;
		playerJoinedEvent.playerId = packet.playerId;
		playerJoinedEvent.x = packet.spawnX;
		playerJoinedEvent.y = packet.spawnY;

		world_->ApplyPlayerJoinedEvent(playerJoinedEvent);
	}

	void UdpClient::HandleJoinRoomResponse(const common::packet::JoinRoomResponsePacket& packet)
	{
		if (world_ == nullptr)
		{
			return;
		}

		const RoomId previousRoomId = world_->GetCurrentRoomId();
		if (previousRoomId != 0)
		{
			snapshotChunkAssembler_.ResetRoom(previousRoomId);
		}

		world_->SetCurrentRoomId(packet.roomId);
		world_->ResetLocalPlayerPrediction(packet.spawnX, packet.spawnY);

		const PlayerId localPlayerId = world_->GetLocalPlayerId();
		if (localPlayerId != 0)
		{
			game::ClientWorld::PlayerJoinedEvent playerJoinedEvent;
			playerJoinedEvent.playerId = localPlayerId;
			playerJoinedEvent.x = packet.spawnX;
			playerJoinedEvent.y = packet.spawnY;

			world_->ApplyPlayerJoinedEvent(playerJoinedEvent);
		}
	}

	void UdpClient::HandlePlayerJoined(const common::packet::PlayerJoinedPacket& packet)
	{
		if (world_ == nullptr)
		{
			return;
		}

		const RoomId currentRoomId = world_->GetCurrentRoomId();
		if (currentRoomId != 0 && packet.roomId != currentRoomId)
		{
			return;
		}

		game::ClientWorld::PlayerJoinedEvent playerJoinedEvent;
		playerJoinedEvent.playerId = packet.playerId;
		playerJoinedEvent.x = packet.x;
		playerJoinedEvent.y = packet.y;

		world_->ApplyPlayerJoinedEvent(playerJoinedEvent);
	}

	void UdpClient::HandlePlayerLeft(const common::packet::PlayerLeftPacket& packet)
	{
		if (world_ == nullptr)
		{
			return;
		}

		const RoomId currentRoomId = world_->GetCurrentRoomId();
		if (currentRoomId != 0 && packet.roomId != currentRoomId)
		{
			return;
		}

		world_->ApplyPlayerLeftEvent(packet.playerId);
	}

	void UdpClient::HandlePlayerSnapshot(const common::packet::PlayerSnapshotPacket& packet)
	{
		if (world_ == nullptr)
		{
			return;
		}

		world_->ApplyPlayerSnapshot(packet);
	}

	void UdpClient::HandleBulletSnapshot(const common::packet::BulletSnapshotPacket& packet)
	{
		if (world_ == nullptr)
		{
			return;
		}

		const auto assembledBulletSnapshot = snapshotChunkAssembler_.PushBulletSnapshotChunk(packet);
		if (!assembledBulletSnapshot.has_value())
		{
			return;
		}

		world_->ApplyBulletSnapshotData(
			assembledBulletSnapshot->serverTick,
			assembledBulletSnapshot->roomId,
			assembledBulletSnapshot->bulletStateDataList
		);
	}

	void UdpClient::HandleImpactEffectPacket(const common::packet::ImpactEffectPacket& packet)
	{
		if (world_ == nullptr)
		{
			return;
		}

		const auto assembledBulletSnapshot = snapshotChunkAssembler_.PushImpactEffectChunk(packet);
		if (!assembledBulletSnapshot.has_value())
		{
			return;
		}

		world_->ApplyImpactEffectData(
			assembledBulletSnapshot->serverTick,
			assembledBulletSnapshot->roomId,
			assembledBulletSnapshot->impactEffectDataList
		);
	}

	void UdpClient::SetTransportConfig(config::ClientTransportType transportType, std::size_t iocpWorkerThreadCount, std::size_t iocpRecvContextCount) noexcept
	{
		transportType_ = transportType;

		if (iocpWorkerThreadCount == 0)
		{
			iocpWorkerThreadCount_ = config::defaultIocpWorkerThreadCount;
		}
		else
		{
			iocpWorkerThreadCount_ = iocpWorkerThreadCount;
		}

		if (iocpRecvContextCount == 0)
		{
			iocpRecvContextCount_ = config::defaultIocpRecvContextCount;
		}
		else
		{
			iocpRecvContextCount_ = iocpRecvContextCount;
		}
	}

	void UdpClient::SetSnapshotAssemblyTimeout(std::chrono::milliseconds snapshotAssemblyTimeout) noexcept
	{
		if (snapshotAssemblyTimeout <= std::chrono::milliseconds(0))
		{
			snapshotAssemblyTimeout_ = config::defaultSnapshotAssemblyTimeout;
			return;
		}

		snapshotAssemblyTimeout_ = snapshotAssemblyTimeout;
	}
}