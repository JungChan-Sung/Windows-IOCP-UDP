#pragma once

#include <cstddef>
#include <cstdint>
#include <concepts>
#include <optional>
#include <unordered_map>
#include <utility>
#include <vector>

#include <Common/Time/TimeTypes.h>

namespace common::net
{
	template <typename TData, typename TRoomId>
	struct SnapshotChunkView
	{
	public:
		TRoomId roomId = 0;
		std::uint32_t serverTick = 0;
		std::uint16_t chunkIndex = 0;
		std::uint16_t chunkCount = 0;
		std::vector<TData> dataList;
	};

	template <typename TData, typename TRoomId>
	struct AssembledSnapshotChunk
	{
	public:
		TRoomId roomId = 0;
		std::uint32_t serverTick = 0;
		std::vector<TData> dataList;
	};

	template <typename TData>
	struct SnapshotChunkAssemblyState
	{
	public:
		std::uint32_t serverTick = 0;
		std::uint16_t chunkCount = 0;
		std::uint16_t receivedChunkCount = 0;
		std::vector<std::uint8_t> receivedChunkFlagList;
		std::vector<std::vector<TData>> chunkDataList;
		time::TimePoint lastUpdatedTime;
	};

	template <typename TData, typename TRoomId>
	class SnapshotChunkAssemblerCore
	{
	public:
		using RoomId = TRoomId;
		using Data = TData;
		using DataList = std::vector<Data>;
		using ChunkView = SnapshotChunkView<Data, RoomId>;
		using AssembledChunk = AssembledSnapshotChunk<Data, RoomId>;

	private:
		using AssemblyState = SnapshotChunkAssemblyState<Data>;
		using AssemblyTable = std::unordered_map<RoomId, AssemblyState>;
		using AppliedTickTable = std::unordered_map<RoomId, std::uint32_t>;

	private:
		AssemblyTable assemblyTable_;
		AppliedTickTable lastAppliedTickTable_;

	public:
		SnapshotChunkAssemblerCore() = default;
		~SnapshotChunkAssemblerCore() noexcept = default;

		SnapshotChunkAssemblerCore(const SnapshotChunkAssemblerCore&) = delete;
		SnapshotChunkAssemblerCore& operator=(const SnapshotChunkAssemblerCore&) = delete;

		SnapshotChunkAssemblerCore(SnapshotChunkAssemblerCore&&) = delete;
		SnapshotChunkAssemblerCore& operator=(SnapshotChunkAssemblerCore&&) = delete;

	public:
		void Clear() noexcept
		{
			assemblyTable_.clear();
			lastAppliedTickTable_.clear();
		}

		void ResetRoom(RoomId roomId) noexcept
		{
			assemblyTable_.erase(roomId);
			lastAppliedTickTable_.erase(roomId);
		}

		template <typename TLogFunc>
			requires std::invocable<
				TLogFunc,
					const char*,
					RoomId,
					std::uint32_t,
					std::uint16_t,
					std::uint16_t,
					std::uint16_t,
					std::uint32_t,
					std::size_t
			>
		void CleanupExpiredAssemblies(time::TimePoint currentTime, time::Milliseconds assemblyTimeout, TLogFunc logFunc) noexcept
		{
			for (auto assemblyIterator = assemblyTable_.begin(); assemblyIterator != assemblyTable_.end();)
			{
				if (currentTime - assemblyIterator->second.lastUpdatedTime <= assemblyTimeout)
				{
					++assemblyIterator;
					continue;
				}

				const RoomId roomId = assemblyIterator->first;
				const AssemblyState& assemblyState = assemblyIterator->second;

				const auto appliedTickIterator = lastAppliedTickTable_.find(roomId);
				const std::uint32_t lastAppliedTick
					= (appliedTickIterator != lastAppliedTickTable_.end()) ? appliedTickIterator->second : 0;

				logFunc(
					"TimeoutDrop",
					roomId,
					assemblyState.serverTick,
					0,
					assemblyState.chunkCount,
					assemblyState.receivedChunkCount,
					lastAppliedTick,
					0
				);

				assemblyIterator = assemblyTable_.erase(assemblyIterator);
			}
		}

		template <typename TLogFunc>
			requires std::invocable<
				TLogFunc,
					const char*,
					RoomId,
					std::uint32_t,
					std::uint16_t,
					std::uint16_t,
					std::uint16_t,
					std::uint32_t,
					std::size_t
			>
		[[nodiscard]] std::optional<AssembledChunk> PushChunk(
			const ChunkView& chunkView,
			time::TimePoint currentTime,
			TLogFunc logFunc
		)
		{
			if (chunkView.chunkCount == 0 || chunkView.chunkIndex >= chunkView.chunkCount)
			{
				logFunc(
					"InvalidChunkHeaderDrop",
					chunkView.roomId,
					chunkView.serverTick,
					chunkView.chunkIndex,
					chunkView.chunkCount,
					0,
					0,
					chunkView.dataList.size()
				);
				return std::nullopt;
			}

			const auto appliedTickIterator = lastAppliedTickTable_.find(chunkView.roomId);
			const bool hasAppliedTick = appliedTickIterator != lastAppliedTickTable_.end();
			const std::uint32_t lastAppliedTick = hasAppliedTick ? appliedTickIterator->second : 0;

			const auto assemblyIterator = assemblyTable_.find(chunkView.roomId);
			const bool hasAssemblyState = assemblyIterator != assemblyTable_.end();

			if (hasAppliedTick && chunkView.serverTick < lastAppliedTick)
			{
				logFunc(
					"StaleDrop-OlderThanApplied",
					chunkView.roomId,
					chunkView.serverTick,
					chunkView.chunkIndex,
					chunkView.chunkCount,
					0,
					lastAppliedTick,
					chunkView.dataList.size()
				);
				return std::nullopt;
			}

			if (hasAppliedTick && chunkView.serverTick == lastAppliedTick && !hasAssemblyState)
			{
				logFunc(
					"StaleDrop-AlreadyApplied",
					chunkView.roomId,
					chunkView.serverTick,
					chunkView.chunkIndex,
					chunkView.chunkCount,
					0,
					lastAppliedTick,
					chunkView.dataList.size()
				);
				return std::nullopt;
			}

			AssemblyState& assemblyState = assemblyTable_[chunkView.roomId];

			const bool needReset
				= assemblyState.chunkCount == 0
				|| chunkView.serverTick > assemblyState.serverTick
				|| chunkView.chunkCount != assemblyState.chunkCount;

			if (needReset)
			{
				assemblyState.serverTick = chunkView.serverTick;
				assemblyState.chunkCount = chunkView.chunkCount;
				assemblyState.receivedChunkCount = 0;
				assemblyState.receivedChunkFlagList.assign(chunkView.chunkCount, 0);
				assemblyState.chunkDataList.assign(chunkView.chunkCount, {});
				assemblyState.lastUpdatedTime = currentTime;

				logFunc(
					"ResetAssembly",
					chunkView.roomId,
					chunkView.serverTick,
					chunkView.chunkIndex,
					chunkView.chunkCount,
					assemblyState.receivedChunkCount,
					lastAppliedTick,
					chunkView.dataList.size()
				);
			}
			else if (chunkView.serverTick < assemblyState.serverTick)
			{
				logFunc(
					"StaleDrop-OlderThanActiveAssembly",
					chunkView.roomId,
					chunkView.serverTick,
					chunkView.chunkIndex,
					chunkView.chunkCount,
					assemblyState.receivedChunkCount,
					lastAppliedTick,
					chunkView.dataList.size()
				);
				return std::nullopt;
			}
			else
			{
				assemblyState.lastUpdatedTime = currentTime;
			}

			if (assemblyState.receivedChunkFlagList[chunkView.chunkIndex] != 0)
			{
				logFunc(
					"DuplicateChunkDrop",
					chunkView.roomId,
					chunkView.serverTick,
					chunkView.chunkIndex,
					chunkView.chunkCount,
					assemblyState.receivedChunkCount,
					lastAppliedTick,
					chunkView.dataList.size()
				);
				return std::nullopt;
			}

			std::vector<Data>& chunkDataList = assemblyState.chunkDataList[chunkView.chunkIndex];
			chunkDataList = chunkView.dataList;

			assemblyState.receivedChunkFlagList[chunkView.chunkIndex] = 1;
			++assemblyState.receivedChunkCount;

			logFunc(
				"AcceptedChunk",
				chunkView.roomId,
				chunkView.serverTick,
				chunkView.chunkIndex,
				chunkView.chunkCount,
				assemblyState.receivedChunkCount,
				lastAppliedTick,
				chunkView.dataList.size()
			);

			if (assemblyState.receivedChunkCount < assemblyState.chunkCount)
			{
				logFunc(
					"WaitingMoreChunks",
					chunkView.roomId,
					chunkView.serverTick,
					chunkView.chunkIndex,
					chunkView.chunkCount,
					assemblyState.receivedChunkCount,
					lastAppliedTick,
					chunkView.dataList.size()
				);
				return std::nullopt;
			}

			AssembledChunk assembledChunk{};
			assembledChunk.roomId = chunkView.roomId;
			assembledChunk.serverTick = assemblyState.serverTick;
			assembledChunk.dataList = FlattenChunkDataList(assemblyState.chunkDataList);

			lastAppliedTickTable_[chunkView.roomId] = assemblyState.serverTick;
			assemblyTable_.erase(chunkView.roomId);

			logFunc(
				"AssembledComplete",
				assembledChunk.roomId,
				assembledChunk.serverTick,
				0,
				chunkView.chunkCount,
				chunkView.chunkCount,
				assembledChunk.serverTick,
				0
			);

			return assembledChunk;
		}

	private:
		[[nodiscard]] static DataList FlattenChunkDataList(const std::vector<std::vector<Data>>& chunkDataList)
		{
			DataList flattenedDataList;

			std::size_t totalCount = 0;
			for (const std::vector<Data>& chunkData : chunkDataList)
			{
				totalCount += chunkData.size();
			}

			flattenedDataList.reserve(totalCount);

			for (const std::vector<Data>& chunkData : chunkDataList)
			{
				flattenedDataList.insert(flattenedDataList.end(), chunkData.begin(), chunkData.end());
			}

			return flattenedDataList;
		}
	};
}