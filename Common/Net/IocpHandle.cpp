#include "IocpHandle.h"

#include <utility>

namespace common::net
{
	IocpHandle::IocpHandle(HANDLE handle) noexcept
		: handle_(handle)
	{}

	IocpHandle::~IocpHandle() noexcept
	{
		Close();
	}

	IocpHandle::IocpHandle(IocpHandle&& other) noexcept
		: handle_(std::exchange(other.handle_, nullptr))
	{}

	IocpHandle& IocpHandle::operator=(IocpHandle&& other) noexcept
	{
		if (this == &other)
		{
			return *this;
		}

		Close();

		handle_ = std::exchange(other.handle_, nullptr);
		return *this;
	}

	void IocpHandle::Reset(HANDLE handle) noexcept
	{
		if (handle_ == handle)
		{
			return;
		}

		Close();

		handle_ = handle;
	}

	HANDLE IocpHandle::Release() noexcept
	{
		return std::exchange(handle_, nullptr);
	}

	void IocpHandle::Close() noexcept
	{
		if (!IsValid())
		{
			return;
		}

		::CloseHandle(handle_);
		handle_ = nullptr;
	}
}