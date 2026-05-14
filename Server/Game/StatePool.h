#pragma once

#include <cstddef>
#include <vector>

namespace server::game
{
	template <typename TState>
	class StatePool
	{
	public:
		using StateList = std::vector<TState>;

	private:
		StateList activeStateList_;

	public:
		StatePool() = default;
		~StatePool() noexcept = default;

		StatePool(const StatePool&) = delete;
		StatePool& operator=(const StatePool&) = delete;

		StatePool(StatePool&&) noexcept = default;
		StatePool& operator=(StatePool&&) noexcept = default;

	public:
		void Clear() noexcept
		{
			activeStateList_.clear();
		}
		void Reserve(std::size_t activeCapacity, std::size_t recycledCapacity)
		{
			activeStateList_.reserve(activeCapacity);
		}

		TState& Add(TState state)
		{
			activeStateList_.push_back(std::move(state));
			return activeStateList_.back();
		}
		void RemoveAt(std::size_t index)
		{
			if (index >= activeStateList_.size())
			{
				return;
			}

			if (index + 1 < activeStateList_.size())
			{
				activeStateList_[index] = std::move(activeStateList_.back());
			}

			activeStateList_.pop_back();
		}

		void RecycleAll()
		{
			activeStateList_.clear();
		}

	public:
		[[nodiscard]] StateList& GetActiveStateList() noexcept
		{
			return activeStateList_;
		}

		[[nodiscard]] const StateList& GetActiveStateList() const noexcept
		{
			return activeStateList_;
		}

		[[nodiscard]] std::size_t GetActiveCount() const noexcept
		{
			return activeStateList_.size();
		}

		[[nodiscard]] std::size_t GetCapacity() const noexcept
		{
			return activeStateList_.capacity();
		}
	};
}