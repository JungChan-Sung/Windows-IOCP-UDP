#pragma once

#include <cstddef>
#include <unordered_map>
#include <utility>
#include <vector>

#include <Common/Net/Endpoint.h>
#include <Common/Net/Reliable/ReliableUdpConfig.h>
#include <Common/Net/Reliable/ReliableUdpSession.h>
#include <Common/Packet/PacketBuffer.h>
#include <Common/Time/TimeTypes.h>

namespace server::net
{
	class ReliableUdpSessionRegistry final
	{
	public:
		using EndpointKey = common::net::EndpointKey;

		struct ResendTask
		{
		public:
			EndpointKey endpointKey{};
			common::packet::PacketBuffer packetBuffer;
		};

		using ResendTaskList = std::vector<ResendTask>;

		struct ResendBatch
		{
		public:
			ResendTaskList taskList;
			std::size_t giveUpPacketCount = 0;
		};

	private:
		using SessionTable = std::unordered_map<EndpointKey, common::net::ReliableUdpSession, common::net::EndpointKeyHasher>;

	private:
		SessionTable sessionTable_;

	public:
		ReliableUdpSessionRegistry() = default;
		~ReliableUdpSessionRegistry() noexcept = default;

		ReliableUdpSessionRegistry(const ReliableUdpSessionRegistry&) = delete;
		ReliableUdpSessionRegistry& operator=(const ReliableUdpSessionRegistry&) = delete;

		ReliableUdpSessionRegistry(ReliableUdpSessionRegistry&&) = delete;
		ReliableUdpSessionRegistry& operator=(ReliableUdpSessionRegistry&&) = delete;

	public:
		[[nodiscard]] common::net::ReliableUdpSession& Upsert(const EndpointKey& endpointKey, const common::net::ReliableUdpConfig& config)
		{
			auto [sessionIterator, inserted] = sessionTable_.try_emplace(endpointKey);
			if (!inserted)
			{
				sessionIterator->second.Reset();
			}

			sessionIterator->second.Configure(config);
			return sessionIterator->second;
		}

		[[nodiscard]] common::net::ReliableUdpSession* Find(const EndpointKey& endpointKey) noexcept
		{
			const auto sessionIterator = sessionTable_.find(endpointKey);
			return (sessionIterator != sessionTable_.end()) ? &sessionIterator->second : nullptr;
		}

		[[nodiscard]] const common::net::ReliableUdpSession* Find(const EndpointKey& endpointKey) const noexcept
		{
			const auto sessionIterator = sessionTable_.find(endpointKey);
			return (sessionIterator != sessionTable_.end()) ? &sessionIterator->second : nullptr;
		}

		[[nodiscard]] bool Remove(const EndpointKey& endpointKey)
		{
			return sessionTable_.erase(endpointKey) > 0;
		}

		void Clear() noexcept
		{
			sessionTable_.clear();
		}

		[[nodiscard]] ResendBatch ExtractResendBatch(common::time::TimePoint currentTime)
		{
			ResendBatch batch{};

			for (auto& [endpointKey, session] : sessionTable_)
			{
				common::net::ReliableUdpSession::ResendResult resendResult = session.ExtractResendResult(currentTime);
				batch.giveUpPacketCount += resendResult.giveUpPacketList.size();

				for (common::net::ReliablePendingPacket& pendingPacket : resendResult.resendPacketList)
				{
					batch.taskList.push_back(ResendTask{
						.endpointKey = endpointKey,
						.packetBuffer = std::move(pendingPacket.packetBuffer),
						});
				}
			}

			return batch;
		}

	public:
		[[nodiscard]] std::size_t GetCount() const noexcept
		{
			return sessionTable_.size();
		}
	};
}