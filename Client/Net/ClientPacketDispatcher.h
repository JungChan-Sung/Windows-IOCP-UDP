#pragma once

#include <functional>
#include <unordered_map>

#include <Common/Packet/PacketHeader.h>

namespace client::net
{
	class ClientPacketDispatcher
	{
	public:
		using PacketHandler = std::function<void(const char*, int)>;

	private:
		struct PacketTypeHasher
		{
		public:
			[[nodiscard]] std::size_t operator()(common::packet::PacketType packetType) const noexcept;
		};

		struct HandlerEntry
		{
		public:
			int expectedPacketSize = 0;
			PacketHandler packetHandler;
		};

	private:
		using HandlerTable = std::unordered_map<common::packet::PacketType, HandlerEntry, PacketTypeHasher>;

	private:
		HandlerTable handlerTable_;

	public:
		ClientPacketDispatcher() = default;
		~ClientPacketDispatcher() noexcept = default;

		ClientPacketDispatcher(const ClientPacketDispatcher&) = delete;
		ClientPacketDispatcher& operator=(const ClientPacketDispatcher&) = delete;

		ClientPacketDispatcher(ClientPacketDispatcher&&) = delete;
		ClientPacketDispatcher& operator=(ClientPacketDispatcher&&) = delete;

	public:
		void Clear() noexcept;

		void RegisterHandler(
			common::packet::PacketType packetType,
			int expectedPacketSize,
			PacketHandler packetHandler
		);

		void Dispatch(const char* packetData, int packetSize) const;
	};
}