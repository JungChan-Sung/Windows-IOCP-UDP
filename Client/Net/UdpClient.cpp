#include "UdpClient.h"

#include <MSWSock.h>
#include <Mstcpip.h>
#include <WS2tcpip.h>

#include <chrono>
#include <thread>
#include <functional>
#include <type_traits>
#include <optional>

#include <Common/Packet/GamePacket.h>
#include <Common/Packet/PacketSerialization.h>
#include <Common/Net/UdpContext.h>

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

		if (!CreateSocket())
		{
			return false;
		}

		if (!BindSocket())
		{
			socket_.Close();
			return false;
		}

		if (!ConfigureSocket())
		{
			socket_.Close();
			return false;
		}

		if (!SetServerAddress(serverIp, serverPort))
		{
			socket_.Close();
			return false;
		}

		world_ = &world;
		inputSequence_ = 0;
		packetDispatcher_.Clear();
		snapshotChunkAssembler_.Clear();
		snapshotChunkAssembler_.SetAssemblyTimeout(snapshotAssemblyTimeout_);
		RegisterPacketHandlers();

		isRunning_.store(true);

		recvThread_ = std::jthread(
			[this](std::stop_token stopToken)
			{
				RecvLoop(stopToken);
			}
		);

		return true;
	}

	void UdpClient::Stop() noexcept
	{
		if (!isRunning_.exchange(false))
		{
			return;
		}

		if (recvThread_.joinable())
		{
			recvThread_.request_stop();
		}

		socket_.Close();

		if (recvThread_.joinable())
		{
			recvThread_.join();
		}

		recvThread_ = std::jthread();

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

	bool UdpClient::CreateSocket()
	{
		SOCKET handle = ::WSASocketW(
			AF_INET,
			SOCK_DGRAM,
			IPPROTO_UDP,
			nullptr,
			0,
			0
		);

		if (handle == INVALID_SOCKET)
		{
			return false;
		}

		socket_.Reset(handle);
		return true;
	}

	bool UdpClient::BindSocket()
	{
		sockaddr_in localAddress{};
		localAddress.sin_family = AF_INET;
		localAddress.sin_addr.s_addr = ::htonl(INADDR_ANY);
		localAddress.sin_port = ::htons(0);

		const int result = ::bind(
			socket_.Get(),
			reinterpret_cast<const sockaddr*>(&localAddress),
			sizeof(localAddress)
		);

		return result != SOCKET_ERROR;
	}

	bool UdpClient::ConfigureSocket()
	{
		u_long nonBlocking = 1;

		const int nonBlockingResult = ::ioctlsocket(
			socket_.Get(),
			FIONBIO,
			&nonBlocking
		);

		if (nonBlockingResult == SOCKET_ERROR)
		{
			return false;
		}

		BOOL newBehavior = FALSE;
		DWORD bytesReturned = 0;

		const int connResetResult = ::WSAIoctl(
			socket_.Get(),
			SIO_UDP_CONNRESET,
			&newBehavior,
			sizeof(newBehavior),
			nullptr,
			0,
			&bytesReturned,
			nullptr,
			nullptr
		);

		return connResetResult != SOCKET_ERROR;
	}

	bool UdpClient::SetServerAddress(const char* serverIp, unsigned short serverPort)
	{
		sockaddr_in serverAddress{};
		serverAddress.sin_family = AF_INET;
		serverAddress.sin_port = ::htons(serverPort);

		const int result = ::InetPtonA(
			AF_INET,
			serverIp,
			&serverAddress.sin_addr
		);

		if (result != 1)
		{
			return false;
		}

		serverAddress_ = serverAddress;
		return true;
	}

	bool UdpClient::SendPacket(const void* packetData, int packetSize)
	{
		if (!socket_.IsValid() || packetData == nullptr || packetSize <= 0)
		{
			return false;
		}

		const int sentBytes = ::sendto(
			socket_.Get(),
			static_cast<const char*>(packetData),
			packetSize,
			0,
			reinterpret_cast<const sockaddr*>(&serverAddress_),
			sizeof(serverAddress_)
		);

		if (sentBytes == SOCKET_ERROR)
		{
			const int errorCode = ::WSAGetLastError();
			if (errorCode == WSAEWOULDBLOCK)
			{
				return false;
			}

			return false;
		}

		return sentBytes == packetSize;
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

	void UdpClient::RecvLoop(std::stop_token stopToken)
	{
		using namespace std::chrono_literals;

		common::net::UdpBuffer receiveBuffer{};

		while (!stopToken.stop_requested())
		{
			sockaddr_in remoteAddress{};
			int remoteAddressLength = static_cast<int>(sizeof(remoteAddress));

			const int receivedBytes = ::recvfrom(
				socket_.Get(),
				receiveBuffer.data(),
				static_cast<int>(receiveBuffer.size()),
				0,
				reinterpret_cast<sockaddr*>(&remoteAddress),
				&remoteAddressLength
			);

			if (receivedBytes == SOCKET_ERROR)
			{
				if (!isRunning_.load())
				{
					break;
				}

				const int errorCode = ::WSAGetLastError();
				if (errorCode == WSAEWOULDBLOCK)
				{
					std::this_thread::sleep_for(1ms);
					continue;
				}

				if (errorCode == WSAENOTSOCK || errorCode == WSAESHUTDOWN || errorCode == WSAEINTR)
				{
					break;
				}

				continue;
			}

			if (receivedBytes <= 0)
			{
				std::this_thread::sleep_for(1ms);
				continue;
			}

			if (remoteAddress.sin_addr.S_un.S_addr != serverAddress_.sin_addr.S_un.S_addr)
			{
				continue;
			}

			if (remoteAddress.sin_port != serverAddress_.sin_port)
			{
				continue;
			}

			HandlePacket(receiveBuffer.data(), receivedBytes);
		}
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