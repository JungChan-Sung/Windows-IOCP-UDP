#pragma once

#include <Windows.h>

namespace common::net
{
	class IocpHandle
	{
	private:
		HANDLE handle_ = nullptr;

	public:
		IocpHandle() = default;
		explicit IocpHandle(HANDLE handle) noexcept;
		~IocpHandle() noexcept;

		IocpHandle(const IocpHandle&) = delete;
		IocpHandle& operator=(const IocpHandle&) = delete;

		IocpHandle(IocpHandle&& other) noexcept;
		IocpHandle& operator=(IocpHandle&& other) noexcept;

	public:
		void Reset(HANDLE handle = nullptr) noexcept;
		[[nodiscard]] HANDLE Release() noexcept;
		void Close() noexcept;

	public:
		[[nodiscard]] bool IsValid() const noexcept
		{
			return handle_ != nullptr;
		}

		[[nodiscard]] HANDLE Get() const noexcept
		{
			return handle_;
		}
	};
}