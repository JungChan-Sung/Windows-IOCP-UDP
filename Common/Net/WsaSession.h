#pragma once

namespace common::net
{
	class WsaSession
	{
	private:
		bool isInitialized_ = false;

	public:
		WsaSession() = default;
		~WsaSession() noexcept;

		WsaSession(const WsaSession&) = delete;
		WsaSession& operator=(const WsaSession&) = delete;

		WsaSession(WsaSession&&) = delete;
		WsaSession& operator=(WsaSession&&) = delete;

	public:
		[[nodiscard]] bool Initialize() noexcept;

	public:
		[[nodiscard]] bool IsInitialized() const noexcept
		{
			return isInitialized_;
		}
	};
}
