#include "UdpIocpTransportMetrics.h"

namespace server::net
{
	void UdpIocpTransportMetrics::Reset() noexcept
	{
		sendCompletionCount_.store(0);
		sendCompletionFailureCount_.store(0);
		sendCompletedByteCount_.store(0);
	}

	void UdpIocpTransportMetrics::RecordSendCompletion(std::uint64_t completedByteCount) noexcept
	{
		++sendCompletionCount_;
		sendCompletedByteCount_.fetch_add(completedByteCount);
	}

	void UdpIocpTransportMetrics::RecordSendCompletionFailure() noexcept
	{
		++sendCompletionFailureCount_;
	}

	UdpIocpTransportMetricsSnapshot UdpIocpTransportMetrics::CaptureSnapshot() const noexcept
	{
		UdpIocpTransportMetricsSnapshot snapshot{};

		snapshot.sendCompletionCount = sendCompletionCount_.load();
		snapshot.sendCompletionFailureCount = sendCompletionFailureCount_.load();
		snapshot.sendCompletedByteCount = sendCompletedByteCount_.load();

		return snapshot;
	}
}
