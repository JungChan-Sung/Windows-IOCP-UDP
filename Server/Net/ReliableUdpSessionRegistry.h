#pragma once

#include <cstddef>
#include <expected>
#include <optional>
#include <unordered_map>
#include <unordered_set>
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

		enum class ProcessReceivedPacketStatus
		{
			SessionNotFound,
			AckOnlyProcessed,
			InvalidAck,
			DataReceived,
			DuplicateData,
		};

		struct ProcessReceivedPacketResult
		{
		public:
			ProcessReceivedPacketStatus status = ProcessReceivedPacketStatus::SessionNotFound;
			std::optional<common::packet::PacketBuffer> ackPacketBuffer;
		};

		enum class BuildOutgoingPacketFailure
		{
			SessionNotFound,
			InvalidGamePacket,
			SendWindowFull,
		};

		using BuildOutgoingPacketResult = std::expected<common::packet::PacketBuffer, BuildOutgoingPacketFailure>;

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
		using ClosingEndpointSet = std::unordered_set<EndpointKey, common::net::EndpointKeyHasher>;

	private:
		SessionTable sessionTable_;
		ClosingEndpointSet closingEndpointSet_;

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

			closingEndpointSet_.erase(endpointKey);

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

		[[nodiscard]] ProcessReceivedPacketResult ProcessReceivedPacket(
			const EndpointKey& endpointKey,
			const common::net::ReliableUdpPacketView& packetView
		)
		{
			common::net::ReliableUdpSession* session = Find(endpointKey);
			if (session == nullptr)
			{
				return ProcessReceivedPacketResult{
					.status = ProcessReceivedPacketStatus::SessionNotFound,
				};
			}

			common::net::ReliableUdpSession::ProcessReceivedPacketResult sessionResult = session->ProcessReceivedPacket(packetView);

			ProcessReceivedPacketStatus status = ProcessReceivedPacketStatus::SessionNotFound;

			using SessionStatus = common::net::ReliableUdpSession::ProcessReceivedPacketStatus;

			switch (sessionResult.status)
			{
			case SessionStatus::AckOnlyProcessed:
				status = ProcessReceivedPacketStatus::AckOnlyProcessed;
				break;

			case SessionStatus::InvalidAck:
				status = ProcessReceivedPacketStatus::InvalidAck;
				break;

			case SessionStatus::DataReceived:
				status = ProcessReceivedPacketStatus::DataReceived;
				break;

			case SessionStatus::DuplicateData:
				status = ProcessReceivedPacketStatus::DuplicateData;
				break;
			}

			const bool shouldCloseSession = closingEndpointSet_.contains(endpointKey) && session->GetPendingPacketCount() == 0;
			if (shouldCloseSession)
			{
				closingEndpointSet_.erase(endpointKey);
				sessionTable_.erase(endpointKey);
			}

			return ProcessReceivedPacketResult{
				.status = status,
				.ackPacketBuffer = std::move(sessionResult.ackPacketBuffer),
			};
		}

		[[nodiscard]] BuildOutgoingPacketResult BuildOutgoingPacket(
			const EndpointKey& endpointKey,
			common::packet::ConstPacketSpan serializedGamePacket,
			common::time::TimePoint currentTime
		)
		{
			common::net::ReliableUdpSession* session = Find(endpointKey);
			if (session == nullptr)
			{
				return std::unexpected(BuildOutgoingPacketFailure::SessionNotFound);
			}

			common::net::ReliableUdpSession::BuildOutgoingPacketResult buildResult = session->BuildOutgoingPacket(serializedGamePacket, currentTime);
			if (buildResult.has_value())
			{
				return std::move(*buildResult);
			}

			using SessionFailure = common::net::ReliableUdpSession::BuildOutgoingPacketFailure;

			switch (buildResult.error())
			{
			case SessionFailure::InvalidGamePacket:
				return std::unexpected(BuildOutgoingPacketFailure::InvalidGamePacket);

			case SessionFailure::SendWindowFull:
				return std::unexpected(BuildOutgoingPacketFailure::SendWindowFull);
			}

			return std::unexpected(BuildOutgoingPacketFailure::InvalidGamePacket);
		}

		[[nodiscard]] bool Remove(const EndpointKey& endpointKey)
		{
			closingEndpointSet_.erase(endpointKey);
			return sessionTable_.erase(endpointKey) > 0;
		}

		void Clear() noexcept
		{
			closingEndpointSet_.clear();
			sessionTable_.clear();
		}

		[[nodiscard]] bool BeginClose(const EndpointKey& endpointKey)
		{
			if (!sessionTable_.contains(endpointKey))
			{
				return false;
			}

			closingEndpointSet_.insert(endpointKey);
			return true;
		}

		[[nodiscard]] ResendBatch ExtractResendBatch(common::time::TimePoint currentTime)
		{
			ResendBatch batch{};

			for (auto sessionIterator = sessionTable_.begin(); sessionIterator != sessionTable_.end();)
			{
				const EndpointKey endpointKey = sessionIterator->first;
				common::net::ReliableUdpSession& session = sessionIterator->second;

				common::net::ReliableUdpSession::ResendResult resendResult = session.ExtractResendResult(currentTime);
				batch.giveUpPacketCount += resendResult.giveUpPacketList.size();

				for (common::net::ReliablePendingPacket& pendingPacket : resendResult.resendPacketList)
				{
					batch.taskList.push_back(ResendTask{
						.endpointKey = endpointKey,
						.packetBuffer = std::move(pendingPacket.packetBuffer),
						});
				}

				if (closingEndpointSet_.contains(endpointKey) && session.GetPendingPacketCount() == 0)
				{
					closingEndpointSet_.erase(endpointKey);
					sessionIterator = sessionTable_.erase(sessionIterator);
					continue;
				}

				++sessionIterator;
			}

			return batch;
		}

	public:
		[[nodiscard]] std::size_t GetPendingPacketCount() const noexcept
		{
			std::size_t pendingPacketCount = 0;

			for (const auto& [_, session] : sessionTable_)
			{
				pendingPacketCount += session.GetPendingPacketCount();
			}

			return pendingPacketCount;
		}

		[[nodiscard]] std::size_t GetCount() const noexcept
		{
			return sessionTable_.size();
		}

		[[nodiscard]] bool IsClosing(const EndpointKey& endpointKey) const noexcept
		{
			return closingEndpointSet_.contains(endpointKey);
		}
	};
}