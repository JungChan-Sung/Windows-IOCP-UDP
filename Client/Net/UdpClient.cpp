#include "UdpClient.h"

#include <functional>
#include <type_traits>
#include <optional>

#include <Common/Packet/GamePacket.h>
#include <Common/Packet/PacketSerialization.h>

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

	bool UdpClient::Start(const char* serverIp, unsigned short serverPort, ClientWorldType& world)
	{
		if (isRunning_.load())
		{
			return false;
		}

		world_ = &world;
		inputSequence_ = 0;
		packetDispatcher_.Clear();
		snapshotChunkAssembler_.Clear();
		snapshotChunkAssembler_.SetAssemblyTimeout(snapshotAssemblyTimeout_);
		RegisterPacketHandlers();

		if (!StartTransport(serverIp, serverPort))
		{
			world_ = nullptr;
			packetDispatcher_.Clear();
			snapshotChunkAssembler_.Clear();
			return false;
		}

		isRunning_.store(true);
		return true;
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

		return SendPacket(packetBuffer->data(), static_cast<int>(packetBuffer->size()));
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
		return SendPacket(packetBuffer->data(), static_cast<int>(packetBuffer->size()));
	}

	bool UdpClient::SendFireRequest()
	{
		common::packet::FireRequestPacket packet{};

		const std::optional<common::packet::PacketBuffer> packetBuffer = common::packet::SerializePacket(packet);
		if (!packetBuffer.has_value())
		{
			return false;
		}

		return SendPacket(packetBuffer->data(), static_cast<int>(packetBuffer->size()));
	}

	bool UdpClient::SendLeaveRequest()
	{
		common::packet::LeaveRequestPacket packet{};

		const std::optional<common::packet::PacketBuffer> packetBuffer = common::packet::SerializePacket(packet);
		if (!packetBuffer.has_value())
		{
			return false;
		}

		return SendPacket(packetBuffer->data(), static_cast<int>(packetBuffer->size()));
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

		return SendPacket(packetBuffer->data(), static_cast<int>(packetBuffer->size()));
	}

	bool UdpClient::StartTransport(const char* serverIp, unsigned short serverPort)
	{
		switch (transportType_)
		{
		case config::ClientTransportType::Socket:
			return socketTransport_.Start(
				serverIp,
				serverPort,
				[this](const char* packetData, int packetSize)
				{
					HandlePacket(packetData, packetSize);
				}
			);

		case config::ClientTransportType::Iocp:
			return iocpTransport_.Start(
				serverIp,
				serverPort,
				iocpWorkerThreadCount_,
				iocpRecvContextCount_,
				[this](const char* packetData, int packetSize)
				{
					HandlePacket(packetData, packetSize);
				}
			);

		default:
			return false;
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
		packetDispatcher_.Dispatch(packetData, packetSize);
	}

	void UdpClient::HandleJoinResponse(const common::packet::JoinResponsePacket& packet)
	{
		if (world_ == nullptr)
		{
			return;
		}

		world_->SetJoinState(packet.playerId, 1, packet.spawnX, packet.spawnY);

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