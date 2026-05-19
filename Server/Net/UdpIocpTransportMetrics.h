#pragma once

#include <atomic>
#include <cstdint>

namespace server::net
{
	struct UdpIocpTransportMetricsSnapshot
	{
	public:
		std::uint64_t sendCompletionCount = 0;
		std::uint64_t sendCompletionFailureCount = 0;
		std::uint64_t sendCompletedByteCount = 0;
	};

	class UdpIocpTransportMetrics
	{
	private:
		std::atomic<std::uint64_t> sendCompletionCount_ = 0;
		std::atomic<std::uint64_t> sendCompletionFailureCount_ = 0;
		std::atomic<std::uint64_t> sendCompletedByteCount_ = 0;

	public:
		UdpIocpTransportMetrics() = default;
		~UdpIocpTransportMetrics() noexcept = default;

		UdpIocpTransportMetrics(const UdpIocpTransportMetrics&) = delete;
		UdpIocpTransportMetrics& operator=(const UdpIocpTransportMetrics&) = delete;

		UdpIocpTransportMetrics(UdpIocpTransportMetrics&&) = delete;
		UdpIocpTransportMetrics& operator=(UdpIocpTransportMetrics&&) = delete;

	public:
		void Reset() noexcept;

		void RecordSendCompletion(std::uint64_t completedByteCount) noexcept;
		void RecordSendCompletionFailure() noexcept;

		[[nodiscard]] UdpIocpTransportMetricsSnapshot CaptureSnapshot() const noexcept;
	};
}