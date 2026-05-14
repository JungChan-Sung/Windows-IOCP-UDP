#pragma once

#include <Windows.h>

#include <Common/Game/GameTypes.h>

#include <Client/Game/InputState.h>
#include <Client/Render/GdiObject.h>

namespace client::game
{
	class ClientWorld;
}

namespace client::render
{
	class GdiRenderer;
}

namespace client::ui
{
	class GameWindow
	{
	public:
		using PlayerId = common::game::PlayerId;
		using RoomId = common::game::RoomId;

	private:
		static inline constexpr const wchar_t* windowClassName = L"UdpGameClientWindowClass";
		static inline constexpr UINT_PTR repaintTimerId = 1;
		static inline constexpr UINT repaintIntervalMs = 16;

	private:
		HWND windowHandle_ = nullptr;
		render::GdiMemoryDeviceContext backBufferDeviceContext_;
		render::GdiBitmap backBufferBitmap_;
		render::ScopedSelectObject backBufferBitmapSelection_;
		int backBufferWidth_ = 0;
		int backBufferHeight_ = 0;

		game::InputState inputState_;
		game::ClientWorld* world_ = nullptr;
		render::GdiRenderer* renderer_ = nullptr;

	public:
		GameWindow() = default;
		~GameWindow() noexcept = default;

		GameWindow(const GameWindow&) = delete;
		GameWindow& operator=(const GameWindow&) = delete;

		GameWindow(GameWindow&&) = delete;
		GameWindow& operator=(GameWindow&&) = delete;

	private:
		static LRESULT CALLBACK WindowProc(HWND windowHandle, UINT message, WPARAM wParam, LPARAM lParam);

	public:
		[[nodiscard]] bool Create(
			HINSTANCE instanceHandle,
			game::ClientWorld& world,
			render::GdiRenderer& renderer,
			const wchar_t* windowTitle
		);
		void Destroy() noexcept;

	private:
		LRESULT HandleMessage(UINT message, WPARAM wParam, LPARAM lParam);
		void OnPaint();
		void OnKeyDown(WPARAM wParam) noexcept;
		void OnKeyUp(WPARAM wParam) noexcept;
		void OnKillFocus() noexcept;
		void OnTimer(WPARAM wParam) noexcept;
		void OnSize(LPARAM lParam) noexcept;

		[[nodiscard]] bool CreateBackBuffer(int width, int height);
		void DestroyBackBuffer() noexcept;

	public:
		[[nodiscard]] HWND GetWindowHandle() const noexcept
		{
			return windowHandle_;
		}

		[[nodiscard]] const game::InputState& GetInputState() const noexcept
		{
			return inputState_;
		}
	};
}

