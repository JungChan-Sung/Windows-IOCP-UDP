#pragma once

#include <expected>

namespace common::net
{
	// WinSock - 초기화 및 정리 RAII 클래스
	class WsaSession
	{
	public:
		struct InitializeError
		{
		public:
			int errorCode = 0;
		};

	public:
		using InitializeResult = std::expected<void, InitializeError>;

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
		[[nodiscard]] InitializeResult Initialize() noexcept;

	public:
		[[nodiscard]] bool IsInitialized() const noexcept
		{
			return isInitialized_;
		}
	};
}
