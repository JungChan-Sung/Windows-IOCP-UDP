#include "GameWindow.h"

#include <utility>

#include <Common/Time/TimeTypes.h>

#include <Client/Game/ClientWorld.h>
#include <Client/Render/GdiRenderer.h>

namespace client::ui
{
	LRESULT GameWindow::WindowProc(HWND windowHandle, UINT message, WPARAM wParam, LPARAM lParam)
	{
		GameWindow* gameWindow = nullptr;

		if (message == WM_NCCREATE)
		{
			const CREATESTRUCTW* createStruct = reinterpret_cast<const CREATESTRUCTW*>(lParam);
			gameWindow = static_cast<GameWindow*>(createStruct->lpCreateParams);

			::SetWindowLongPtrW(windowHandle, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(gameWindow));
			gameWindow->windowHandle_ = windowHandle;
		}
		else
		{
			gameWindow = reinterpret_cast<GameWindow*>(::GetWindowLongPtrW(windowHandle, GWLP_USERDATA));
		}

		if (gameWindow == nullptr)
		{
			return ::DefWindowProcW(windowHandle, message, wParam, lParam);
		}

		return gameWindow->HandleMessage(message, wParam, lParam);
	}

	bool GameWindow::Create(HINSTANCE instanceHandle, game::ClientWorld& world, render::GdiRenderer& renderer, const wchar_t* windowTitle)
	{
		world_ = &world;
		renderer_ = &renderer;

		WNDCLASSW windowClass{};
		windowClass.lpfnWndProc = &GameWindow::WindowProc;
		windowClass.hInstance = instanceHandle;
		windowClass.lpszClassName = windowClassName;
		windowClass.hCursor = ::LoadCursorW(nullptr, IDC_ARROW);

		if (::RegisterClassW(&windowClass) == 0)
		{
			const DWORD errorCode = ::GetLastError();
			if (errorCode != ERROR_CLASS_ALREADY_EXISTS)
			{
				world_ = nullptr;
				renderer_ = nullptr;
				return false;
			}
		}

		windowHandle_ = ::CreateWindowExW(
			0,
			windowClassName,
			windowTitle,
			WS_OVERLAPPEDWINDOW,
			CW_USEDEFAULT,
			CW_USEDEFAULT,
			1280,
			720,
			nullptr,
			nullptr,
			instanceHandle,
			this
		);

		if (windowHandle_ == nullptr)
		{
			world_ = nullptr;
			renderer_ = nullptr;
			return false;
		}

		::ShowWindow(windowHandle_, SW_SHOW);
		::UpdateWindow(windowHandle_);

		return true;
	}

	void GameWindow::Destroy() noexcept
	{
		if (windowHandle_ != nullptr)
		{
			::DestroyWindow(windowHandle_);
			windowHandle_ = nullptr;
		}

		DestroyBackBuffer();

		world_ = nullptr;
		renderer_ = nullptr;
		inputState_.Clear();
	}

	void GameWindow::RequestClose() const noexcept
	{
		if (windowHandle_ == nullptr)
		{
			return;
		}

		::PostMessageW(windowHandle_, WM_CLOSE, 0, 0);
	}

	LRESULT GameWindow::HandleMessage(UINT message, WPARAM wParam, LPARAM lParam)
	{
		switch (message)
		{
		case WM_CREATE:
			::SetTimer(windowHandle_, repaintTimerId, repaintIntervalMs, nullptr);
			return 0;

		case WM_SIZE:
			OnSize(lParam);
			return 0;

		case WM_ERASEBKGND:
			return 1;

		case WM_PAINT:
			OnPaint();
			return 0;

		case WM_TIMER:
			OnTimer(wParam);
			return 0;

		case WM_KEYDOWN:
			OnKeyDown(wParam, lParam);
			return 0;

		case WM_KEYUP:
			OnKeyUp(wParam);
			return 0;

		case WM_KILLFOCUS:
			OnKillFocus();
			return 0;

		case WM_DESTROY:
			::KillTimer(windowHandle_, repaintTimerId);
			DestroyBackBuffer();
			inputState_.Clear();
			::PostQuitMessage(0);
			return 0;

		default:
			return ::DefWindowProcW(windowHandle_, message, wParam, lParam);
		}
	}

	void GameWindow::OnPaint()
	{
		render::PaintDeviceContext paintDeviceContext(windowHandle_);
		HDC windowDeviceContext = paintDeviceContext.Get();
		if (windowDeviceContext == nullptr)
		{
			return;
		}

		RECT clientRect{};
		::GetClientRect(windowHandle_, &clientRect);

		const int clientWidth = clientRect.right - clientRect.left;
		const int clientHeight = clientRect.bottom - clientRect.top;

		if (clientWidth <= 0 || clientHeight <= 0)
		{
			return;
		}

		const bool isBackBufferReady
			= backBufferDeviceContext_.IsValid()
			&& backBufferBitmap_.IsValid()
			&& backBufferWidth_ == clientWidth
			&& backBufferHeight_ == clientHeight;

		if (!isBackBufferReady && !CreateBackBuffer(clientWidth, clientHeight))
		{
			return;
		}

		if (world_ == nullptr || renderer_ == nullptr)
		{
			return;
		}

		const game::ClientWorld::RenderFrameSnapshot renderFrameSnapshot = world_->BuildRenderFrameSnapshot(common::time::Clock::now());
		renderer_->Render(
			backBufferDeviceContext_.Get(),
			clientRect,
			renderFrameSnapshot
		);

		::BitBlt(
			windowDeviceContext,
			0,
			0,
			clientWidth,
			clientHeight,
			backBufferDeviceContext_.Get(),
			0,
			0,
			SRCCOPY
		);
	}

	void GameWindow::OnKeyDown(WPARAM wParam, LPARAM lParam) noexcept
	{
		const bool isRepeat = (lParam & (static_cast<LPARAM>(1) << 30)) != 0;

		inputState_.SetKeyDown(static_cast<unsigned int>(wParam), isRepeat);
	}

	void GameWindow::OnKeyUp(WPARAM wParam) noexcept
	{
		inputState_.SetKeyUp(static_cast<unsigned int>(wParam));
	}

	void GameWindow::OnKillFocus() noexcept
	{
		inputState_.Clear();
	}

	void GameWindow::OnTimer(WPARAM wParam) noexcept
	{
		if (wParam != repaintTimerId)
		{
			return;
		}

		::InvalidateRect(windowHandle_, nullptr, FALSE);
	}

	void GameWindow::OnSize(LPARAM lParam) noexcept
	{
		const int clientWidth = LOWORD(lParam);
		const int clientHeight = HIWORD(lParam);

		if (clientWidth <= 0 || clientHeight <= 0)
		{
			DestroyBackBuffer();
			return;
		}

		CreateBackBuffer(clientWidth, clientHeight);
	}

	bool GameWindow::CreateBackBuffer(int width, int height)
	{
		if (windowHandle_ == nullptr || width <= 0 || height <= 0)
		{
			return false;
		}

		DestroyBackBuffer();

		render::WindowDeviceContext windowDeviceContext(windowHandle_);
		if (!windowDeviceContext.IsValid())
		{
			return false;
		}

		render::GdiMemoryDeviceContext newBackBufferDeviceContext(::CreateCompatibleDC(windowDeviceContext.Get()));
		if (!newBackBufferDeviceContext.IsValid())
		{
			return false;
		}

		render::GdiBitmap newBackBufferBitmap(::CreateCompatibleBitmap(windowDeviceContext.Get(), width, height));
		if (!newBackBufferBitmap.IsValid())
		{
			return false;
		}

		render::ScopedSelectObject newBackBufferBitmapSelection(
			newBackBufferDeviceContext.Get(),
			render::ToGdiObject(newBackBufferBitmap.Get())
		);
		if (!newBackBufferBitmapSelection.IsSelected())
		{
			return false;
		}

		backBufferDeviceContext_ = std::move(newBackBufferDeviceContext);
		backBufferBitmap_ = std::move(newBackBufferBitmap);
		backBufferBitmapSelection_ = std::move(newBackBufferBitmapSelection);

		backBufferWidth_ = width;
		backBufferHeight_ = height;

		return true;
	}

	void GameWindow::DestroyBackBuffer() noexcept
	{
		backBufferWidth_ = 0;
		backBufferHeight_ = 0;

		backBufferBitmapSelection_.Restore();
		backBufferBitmap_.Reset();
		backBufferDeviceContext_.Reset();
	}
}