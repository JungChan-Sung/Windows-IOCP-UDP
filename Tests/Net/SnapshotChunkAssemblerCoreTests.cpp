#include "SnapshotChunkAssemblerCoreTests.h"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <initializer_list>
#include <optional>
#include <string_view>
#include <vector>

#include <Common/Net/SnapshotChunkAssemblerCore.h>

namespace
{
	struct TestData
	{
	public:
		std::uint32_t id = 0;
		float x = 0.0F;
		float y = 0.0F;
	};

	using RoomId = std::int32_t;
	using AssemblerCore = common::net::SnapshotChunkAssemblerCore<TestData, RoomId>;
	using ChunkView = AssemblerCore::ChunkView;
	using AssembledChunk = AssemblerCore::AssembledChunk;
	using LogList = std::vector<std::string_view>;

	void TestLog(
		const char* tag,
		RoomId,
		std::uint32_t,
		std::uint16_t,
		std::uint16_t,
		std::uint16_t,
		std::uint32_t,
		std::size_t
	)
	{
		(void)tag;
	}

	void CaptureLog(
		LogList& logList,
		const char* tag,
		RoomId,
		std::uint32_t,
		std::uint16_t,
		std::uint16_t,
		std::uint16_t,
		std::uint32_t,
		std::size_t
	)
	{
		logList.emplace_back(tag);
	}

	[[nodiscard]] ChunkView MakeChunk(
		RoomId roomId,
		std::uint32_t serverTick,
		std::uint16_t chunkIndex,
		std::uint16_t chunkCount,
		std::initializer_list<TestData> dataList
	)
	{
		ChunkView chunkView{};
		chunkView.roomId = roomId;
		chunkView.serverTick = serverTick;
		chunkView.chunkIndex = chunkIndex;
		chunkView.chunkCount = chunkCount;
		chunkView.dataList.assign(dataList.begin(), dataList.end());

		return chunkView;
	}

	void RunAssembleInOrderTest(common::diagnostics::DebugTestResult& result)
	{
		AssemblerCore assemblerCore;
		const auto now = std::chrono::steady_clock::now();

		const std::optional<AssembledChunk> firstResult = assemblerCore.PushChunk(
			MakeChunk(1, 100, 0, 2, { TestData{ 1, 10.0F, 20.0F } }),
			now,
			TestLog
		);

		common::diagnostics::Expect(result, !firstResult.has_value(), "SnapshotChunkCore: first chunk waits");

		const std::optional<AssembledChunk> secondResult = assemblerCore.PushChunk(
			MakeChunk(1, 100, 1, 2, { TestData{ 2, 30.0F, 40.0F } }),
			now,
			TestLog
		);

		common::diagnostics::Expect(result, secondResult.has_value(), "SnapshotChunkCore: second chunk completes");

		if (!secondResult.has_value())
		{
			return;
		}

		common::diagnostics::Expect(result, secondResult->roomId == 1, "SnapshotChunkCore: assembled roomId");
		common::diagnostics::Expect(result, secondResult->serverTick == 100, "SnapshotChunkCore: assembled serverTick");
		common::diagnostics::Expect(result, secondResult->dataList.size() == 2, "SnapshotChunkCore: assembled data count");
		common::diagnostics::Expect(result, secondResult->dataList[0].id == 1, "SnapshotChunkCore: first data order");
		common::diagnostics::Expect(result, secondResult->dataList[1].id == 2, "SnapshotChunkCore: second data order");
	}

	void RunAssembleOutOfOrderTest(common::diagnostics::DebugTestResult& result)
	{
		AssemblerCore assemblerCore;
		const auto now = std::chrono::steady_clock::now();

		const std::optional<AssembledChunk> firstResult = assemblerCore.PushChunk(
			MakeChunk(1, 101, 1, 2, { TestData{ 20, 30.0F, 40.0F } }),
			now,
			TestLog
		);

		common::diagnostics::Expect(result, !firstResult.has_value(), "SnapshotChunkCore: out of order first waits");

		const std::optional<AssembledChunk> secondResult = assemblerCore.PushChunk(
			MakeChunk(1, 101, 0, 2, { TestData{ 10, 10.0F, 20.0F } }),
			now,
			TestLog
		);

		common::diagnostics::Expect(result, secondResult.has_value(), "SnapshotChunkCore: out of order completes");

		if (!secondResult.has_value())
		{
			return;
		}

		common::diagnostics::Expect(result, secondResult->dataList[0].id == 10, "SnapshotChunkCore: out of order first data");
		common::diagnostics::Expect(result, secondResult->dataList[1].id == 20, "SnapshotChunkCore: out of order second data");
	}

	void RunDuplicateDropTest(common::diagnostics::DebugTestResult& result)
	{
		AssemblerCore assemblerCore;
		LogList logList;
		const auto now = std::chrono::steady_clock::now();

		const ChunkView chunkView = MakeChunk(1, 200, 0, 2, { TestData{ 1, 1.0F, 2.0F } });

		const std::optional<AssembledChunk> firstResult = assemblerCore.PushChunk(
			chunkView,
			now,
			[&logList](const char* tag, RoomId roomId, std::uint32_t serverTick, std::uint16_t chunkIndex, std::uint16_t chunkCount,
				std::uint16_t receivedChunkCount, std::uint32_t lastAppliedTick, std::size_t payloadCount)
			{
				CaptureLog(logList, tag, roomId, serverTick, chunkIndex, chunkCount, receivedChunkCount, lastAppliedTick, payloadCount);
			}
		);

		const std::optional<AssembledChunk> duplicateResult = assemblerCore.PushChunk(
			chunkView,
			now,
			[&logList](const char* tag, RoomId roomId, std::uint32_t serverTick, std::uint16_t chunkIndex, std::uint16_t chunkCount,
				std::uint16_t receivedChunkCount, std::uint32_t lastAppliedTick, std::size_t payloadCount)
			{
				CaptureLog(logList, tag, roomId, serverTick, chunkIndex, chunkCount, receivedChunkCount, lastAppliedTick, payloadCount);
			}
		);

		common::diagnostics::Expect(result, !firstResult.has_value(), "SnapshotChunkCore: duplicate base waits");
		common::diagnostics::Expect(result, !duplicateResult.has_value(), "SnapshotChunkCore: duplicate dropped");

		const bool hasDuplicateLog = std::ranges::find(logList, "DuplicateChunkDrop") != logList.end();
		common::diagnostics::Expect(result, hasDuplicateLog, "SnapshotChunkCore: duplicate log");
	}

	void RunAlreadyAppliedDropTest(common::diagnostics::DebugTestResult& result)
	{
		AssemblerCore assemblerCore;
		LogList logList;
		const auto now = std::chrono::steady_clock::now();

		const std::optional<AssembledChunk> completeResult = assemblerCore.PushChunk(
			MakeChunk(1, 300, 0, 1, { TestData{ 1, 1.0F, 2.0F } }),
			now,
			TestLog
		);

		common::diagnostics::Expect(result, completeResult.has_value(), "SnapshotChunkCore: already applied base completes");

		const std::optional<AssembledChunk> alreadyAppliedResult = assemblerCore.PushChunk(
			MakeChunk(1, 300, 0, 1, { TestData{ 2, 3.0F, 4.0F } }),
			now,
			[&logList](const char* tag, RoomId roomId, std::uint32_t serverTick, std::uint16_t chunkIndex, std::uint16_t chunkCount,
				std::uint16_t receivedChunkCount, std::uint32_t lastAppliedTick, std::size_t payloadCount)
			{
				CaptureLog(logList, tag, roomId, serverTick, chunkIndex, chunkCount, receivedChunkCount, lastAppliedTick, payloadCount);
			}
		);

		common::diagnostics::Expect(result, !alreadyAppliedResult.has_value(), "SnapshotChunkCore: already applied dropped");

		const bool hasAlreadyAppliedLog = std::ranges::find(logList, "StaleDrop-AlreadyApplied") != logList.end();
		common::diagnostics::Expect(result, hasAlreadyAppliedLog, "SnapshotChunkCore: already applied log");
	}

	void RunOlderThanAppliedDropTest(common::diagnostics::DebugTestResult& result)
	{
		AssemblerCore assemblerCore;
		LogList logList;
		const auto now = std::chrono::steady_clock::now();

		const std::optional<AssembledChunk> completeResult = assemblerCore.PushChunk(
			MakeChunk(1, 400, 0, 1, { TestData{ 1, 1.0F, 2.0F } }),
			now,
			TestLog
		);

		common::diagnostics::Expect(result, completeResult.has_value(), "SnapshotChunkCore: stale base completes");

		const std::optional<AssembledChunk> staleResult = assemblerCore.PushChunk(
			MakeChunk(1, 399, 0, 1, { TestData{ 2, 3.0F, 4.0F } }),
			now,
			[&logList](const char* tag, RoomId roomId, std::uint32_t serverTick, std::uint16_t chunkIndex, std::uint16_t chunkCount,
				std::uint16_t receivedChunkCount, std::uint32_t lastAppliedTick, std::size_t payloadCount)
			{
				CaptureLog(logList, tag, roomId, serverTick, chunkIndex, chunkCount, receivedChunkCount, lastAppliedTick, payloadCount);
			}
		);

		common::diagnostics::Expect(result, !staleResult.has_value(), "SnapshotChunkCore: older than applied dropped");

		const bool hasStaleLog = std::ranges::find(logList, "StaleDrop-OlderThanApplied") != logList.end();
		common::diagnostics::Expect(result, hasStaleLog, "SnapshotChunkCore: older than applied log");
	}

	void RunNewerTickResetsAssemblyTest(common::diagnostics::DebugTestResult& result)
	{
		AssemblerCore assemblerCore;
		const auto now = std::chrono::steady_clock::now();

		const std::optional<AssembledChunk> oldResult = assemblerCore.PushChunk(
			MakeChunk(1, 500, 0, 2, { TestData{ 1, 1.0F, 2.0F } }),
			now,
			TestLog
		);

		common::diagnostics::Expect(result, !oldResult.has_value(), "SnapshotChunkCore: old partial waits");

		const std::optional<AssembledChunk> newResult = assemblerCore.PushChunk(
			MakeChunk(1, 501, 0, 1, { TestData{ 2, 3.0F, 4.0F } }),
			now,
			TestLog
		);

		common::diagnostics::Expect(result, newResult.has_value(), "SnapshotChunkCore: newer tick resets and completes");

		if (newResult.has_value())
		{
			common::diagnostics::Expect(result, newResult->serverTick == 501, "SnapshotChunkCore: newer tick serverTick");
			common::diagnostics::Expect(result, newResult->dataList.size() == 1, "SnapshotChunkCore: newer tick data count");
			common::diagnostics::Expect(result, newResult->dataList[0].id == 2, "SnapshotChunkCore: newer tick data");
		}
	}

	void RunInvalidChunkHeaderDropTest(common::diagnostics::DebugTestResult& result)
	{
		AssemblerCore assemblerCore;
		LogList logList;
		const auto now = std::chrono::steady_clock::now();

		const std::optional<AssembledChunk> zeroCountResult = assemblerCore.PushChunk(
			MakeChunk(1, 600, 0, 0, { TestData{ 1, 1.0F, 2.0F } }),
			now,
			[&logList](const char* tag, RoomId roomId, std::uint32_t serverTick, std::uint16_t chunkIndex, std::uint16_t chunkCount,
				std::uint16_t receivedChunkCount, std::uint32_t lastAppliedTick, std::size_t payloadCount)
			{
				CaptureLog(logList, tag, roomId, serverTick, chunkIndex, chunkCount, receivedChunkCount, lastAppliedTick, payloadCount);
			}
		);

		common::diagnostics::Expect(result, !zeroCountResult.has_value(), "SnapshotChunkCore: zero chunkCount rejected");

		const bool hasInvalidLog = std::ranges::find(logList, "InvalidChunkHeaderDrop") != logList.end();
		common::diagnostics::Expect(result, hasInvalidLog, "SnapshotChunkCore: invalid chunk log");
	}

	void RunTimeoutCleanupTest(common::diagnostics::DebugTestResult& result)
	{
		AssemblerCore assemblerCore;
		LogList logList;

		const auto oldTime = std::chrono::steady_clock::now();
		const auto currentTime = oldTime + std::chrono::milliseconds(1000);

		const std::optional<AssembledChunk> partialResult = assemblerCore.PushChunk(
			MakeChunk(1, 700, 0, 2, { TestData{ 1, 1.0F, 2.0F } }),
			oldTime,
			TestLog
		);

		common::diagnostics::Expect(result, !partialResult.has_value(), "SnapshotChunkCore: timeout base waits");

		assemblerCore.CleanupExpiredAssemblies(
			currentTime,
			std::chrono::milliseconds(100),
			[&logList](const char* tag, RoomId roomId, std::uint32_t serverTick, std::uint16_t chunkIndex, std::uint16_t chunkCount,
				std::uint16_t receivedChunkCount, std::uint32_t lastAppliedTick, std::size_t payloadCount)
			{
				CaptureLog(logList, tag, roomId, serverTick, chunkIndex, chunkCount, receivedChunkCount, lastAppliedTick, payloadCount);
			}
		);

		const bool hasTimeoutLog = std::ranges::find(logList, "TimeoutDrop") != logList.end();
		common::diagnostics::Expect(result, hasTimeoutLog, "SnapshotChunkCore: timeout log");

		const std::optional<AssembledChunk> nextResult = assemblerCore.PushChunk(
			MakeChunk(1, 700, 1, 2, { TestData{ 2, 3.0F, 4.0F } }),
			currentTime,
			TestLog
		);

		common::diagnostics::Expect(result, !nextResult.has_value(), "SnapshotChunkCore: chunk after timeout starts new assembly");
	}
}

namespace tests::net
{
	common::diagnostics::DebugTestResult RunSnapshotChunkAssemblerCoreTests()
	{
		common::diagnostics::DebugTestResult result{};

		RunAssembleInOrderTest(result);
		RunAssembleOutOfOrderTest(result);
		RunDuplicateDropTest(result);
		RunAlreadyAppliedDropTest(result);
		RunOlderThanAppliedDropTest(result);
		RunNewerTickResetsAssemblyTest(result);
		RunInvalidChunkHeaderDropTest(result);
		RunTimeoutCleanupTest(result);

		return result;
	}
}