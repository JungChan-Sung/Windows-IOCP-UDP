#pragma once

#include <Windows.h>

#include <utility>

namespace client::render
{
	[[nodiscard]] inline HGDIOBJ ToGdiObject(HBRUSH brush) noexcept
	{
		return reinterpret_cast<HGDIOBJ>(brush);
	}

	[[nodiscard]] inline HGDIOBJ ToGdiObject(HPEN pen) noexcept
	{
		return reinterpret_cast<HGDIOBJ>(pen);
	}

	[[nodiscard]] inline HGDIOBJ ToGdiObject(HBITMAP bitmap) noexcept
	{
		return reinterpret_cast<HGDIOBJ>(bitmap);
	}

	class GdiBrush
	{
	private:
		HBRUSH handle_ = nullptr;

	public:
		GdiBrush() noexcept = default;
		explicit GdiBrush(HBRUSH handle) noexcept
			: handle_(handle)
		{}
		~GdiBrush() noexcept
		{
			Reset();
		}

		GdiBrush(const GdiBrush&) = delete;
		GdiBrush& operator=(const GdiBrush&) = delete;

		GdiBrush(GdiBrush&& other) noexcept
			: handle_(std::exchange(other.handle_, nullptr))
		{}
		GdiBrush& operator=(GdiBrush&& other) noexcept
		{
			if (this == &other)
			{
				return *this;
			}

			Reset();

			handle_ = std::exchange(other.handle_, nullptr);

			return *this;
		}

	public:
		void Reset(HBRUSH handle = nullptr) noexcept
		{
			if (handle_ != nullptr)
			{
				::DeleteObject(ToGdiObject(handle_));
			}

			handle_ = handle;
		}

		[[nodiscard]] HBRUSH Release() noexcept
		{
			return std::exchange(handle_, nullptr);
		}

	public:
		[[nodiscard]] HBRUSH Get() const noexcept
		{
			return handle_;
		}

		[[nodiscard]] bool IsValid() const noexcept
		{
			return handle_ != nullptr;
		}
	};

	class GdiPen
	{
	private:
		HPEN handle_ = nullptr;

	public:
		GdiPen() noexcept = default;
		explicit GdiPen(HPEN handle) noexcept
			: handle_(handle)
		{}
		~GdiPen() noexcept
		{
			Reset();
		}

		GdiPen(const GdiPen&) = delete;
		GdiPen& operator=(const GdiPen&) = delete;

		GdiPen(GdiPen&& other) noexcept
			: handle_(std::exchange(other.handle_, nullptr))
		{}
		GdiPen& operator=(GdiPen&& other) noexcept
		{
			if (this == &other)
			{
				return *this;
			}

			Reset();

			handle_ = std::exchange(other.handle_, nullptr);

			return *this;
		}

	public:
		void Reset(HPEN handle = nullptr) noexcept
		{
			if (handle_ != nullptr)
			{
				::DeleteObject(ToGdiObject(handle_));
			}

			handle_ = handle;
		}

		[[nodiscard]] HPEN Release() noexcept
		{
			return std::exchange(handle_, nullptr);
		}

	public:
		[[nodiscard]] HPEN Get() const noexcept
		{
			return handle_;
		}

		[[nodiscard]] bool IsValid() const noexcept
		{
			return handle_ != nullptr;
		}
	};

	class GdiBitmap
	{
	private:
		HBITMAP handle_ = nullptr;

	public:
		GdiBitmap() noexcept = default;
		explicit GdiBitmap(HBITMAP handle) noexcept
			: handle_(handle)
		{}
		~GdiBitmap() noexcept
		{
			Reset();
		}

		GdiBitmap(const GdiBitmap&) = delete;
		GdiBitmap& operator=(const GdiBitmap&) = delete;

		GdiBitmap(GdiBitmap&& other) noexcept
			: handle_(std::exchange(other.handle_, nullptr))
		{}
		GdiBitmap& operator=(GdiBitmap&& other) noexcept
		{
			if (this == &other)
			{
				return *this;
			}

			Reset();

			handle_ = std::exchange(other.handle_, nullptr);

			return *this;
		}

	public:
		void Reset(HBITMAP handle = nullptr) noexcept
		{
			if (handle_ != nullptr)
			{
				::DeleteObject(ToGdiObject(handle_));
			}

			handle_ = handle;
		}

		[[nodiscard]] HBITMAP Release() noexcept
		{
			return std::exchange(handle_, nullptr);
		}

	public:
		[[nodiscard]] HBITMAP Get() const noexcept
		{
			return handle_;
		}

		[[nodiscard]] bool IsValid() const noexcept
		{
			return handle_ != nullptr;
		}
	};

	class GdiMemoryDeviceContext
	{
	private:
		HDC deviceContext_ = nullptr;

	public:
		GdiMemoryDeviceContext() noexcept = default;
		explicit GdiMemoryDeviceContext(HDC deviceContext) noexcept
			: deviceContext_(deviceContext)
		{}
		~GdiMemoryDeviceContext() noexcept
		{
			Reset();
		}

		GdiMemoryDeviceContext(const GdiMemoryDeviceContext&) = delete;
		GdiMemoryDeviceContext& operator=(const GdiMemoryDeviceContext&) = delete;

		GdiMemoryDeviceContext(GdiMemoryDeviceContext&& other) noexcept
			: deviceContext_(std::exchange(other.deviceContext_, nullptr))
		{}
		GdiMemoryDeviceContext& operator=(GdiMemoryDeviceContext&& other) noexcept
		{
			if (this == &other)
			{
				return *this;
			}

			Reset();

			deviceContext_ = std::exchange(other.deviceContext_, nullptr);

			return *this;
		}

	public:
		void Reset(HDC deviceContext = nullptr) noexcept
		{
			if (deviceContext_ != nullptr)
			{
				::DeleteDC(deviceContext_);
			}

			deviceContext_ = deviceContext;
		}

		[[nodiscard]] HDC Release() noexcept
		{
			return std::exchange(deviceContext_, nullptr);
		}

	public:
		[[nodiscard]] HDC Get() const noexcept
		{
			return deviceContext_;
		}

		[[nodiscard]] bool IsValid() const noexcept
		{
			return deviceContext_ != nullptr;
		}
	};

	class WindowDeviceContext
	{
	private:
		HWND windowHandle_ = nullptr;
		HDC deviceContext_ = nullptr;

	public:
		explicit WindowDeviceContext(HWND windowHandle) noexcept
			: windowHandle_(windowHandle),
			deviceContext_(windowHandle != nullptr ? ::GetDC(windowHandle) : nullptr)
		{}
		~WindowDeviceContext() noexcept
		{
			if (windowHandle_ != nullptr && deviceContext_ != nullptr)
			{
				::ReleaseDC(windowHandle_, deviceContext_);
			}
		}

		WindowDeviceContext(const WindowDeviceContext&) = delete;
		WindowDeviceContext& operator=(const WindowDeviceContext&) = delete;

		WindowDeviceContext(WindowDeviceContext&&) = delete;
		WindowDeviceContext& operator=(WindowDeviceContext&&) = delete;

	public:
		[[nodiscard]] HDC Get() const noexcept
		{
			return deviceContext_;
		}

		[[nodiscard]] bool IsValid() const noexcept
		{
			return deviceContext_ != nullptr;
		}
	};

	class PaintDeviceContext
	{
	private:
		HWND windowHandle_ = nullptr;
		PAINTSTRUCT paintStruct_{};
		HDC deviceContext_ = nullptr;

	public:
		explicit PaintDeviceContext(HWND windowHandle) noexcept
			: windowHandle_(windowHandle),
			deviceContext_(windowHandle != nullptr ? ::BeginPaint(windowHandle, &paintStruct_) : nullptr)
		{}
		~PaintDeviceContext() noexcept
		{
			if (windowHandle_ != nullptr && deviceContext_ != nullptr)
			{
				::EndPaint(windowHandle_, &paintStruct_);
			}
		}

		PaintDeviceContext(const PaintDeviceContext&) = delete;
		PaintDeviceContext& operator=(const PaintDeviceContext&) = delete;

		PaintDeviceContext(PaintDeviceContext&&) = delete;
		PaintDeviceContext& operator=(PaintDeviceContext&&) = delete;

	public:
		[[nodiscard]] HDC Get() const noexcept
		{
			return deviceContext_;
		}

		[[nodiscard]] const PAINTSTRUCT& GetPaintStruct() const noexcept
		{
			return paintStruct_;
		}

		[[nodiscard]] bool IsValid() const noexcept
		{
			return deviceContext_ != nullptr;
		}
	};

	class ScopedSelectObject
	{
	private:
		HDC deviceContext_ = nullptr;
		HGDIOBJ previousObject_ = nullptr;

	public:
		ScopedSelectObject() noexcept = default;
		ScopedSelectObject(HDC deviceContext, HGDIOBJ object) noexcept
		{
			Reset(deviceContext, object);
		}
		~ScopedSelectObject() noexcept
		{
			Restore();
		}

		ScopedSelectObject(const ScopedSelectObject&) = delete;
		ScopedSelectObject& operator=(const ScopedSelectObject&) = delete;

		ScopedSelectObject(ScopedSelectObject&& other) noexcept
			: deviceContext_(std::exchange(other.deviceContext_, nullptr)),
			previousObject_(std::exchange(other.previousObject_, nullptr))
		{}
		ScopedSelectObject& operator=(ScopedSelectObject&& other) noexcept
		{
			if (this == &other)
			{
				return *this;
			}

			Restore();

			deviceContext_ = std::exchange(other.deviceContext_, nullptr);
			previousObject_ = std::exchange(other.previousObject_, nullptr);

			return *this;
		}

	public:
		void Reset(HDC deviceContext, HGDIOBJ object) noexcept
		{
			Restore();

			if (deviceContext == nullptr || object == nullptr)
			{
				return;
			}

			previousObject_ = ::SelectObject(deviceContext, object);
			if (previousObject_ == nullptr || previousObject_ == HGDI_ERROR)
			{
				deviceContext_ = nullptr;
				previousObject_ = nullptr;
				return;
			}

			deviceContext_ = deviceContext;
		}

		void Restore() noexcept
		{
			if (deviceContext_ != nullptr && previousObject_ != nullptr)
			{
				::SelectObject(deviceContext_, previousObject_);
			}

			deviceContext_ = nullptr;
			previousObject_ = nullptr;
		}

	public:
		[[nodiscard]] bool IsSelected() const noexcept
		{
			return deviceContext_ != nullptr && previousObject_ != nullptr;
		}
	};
}