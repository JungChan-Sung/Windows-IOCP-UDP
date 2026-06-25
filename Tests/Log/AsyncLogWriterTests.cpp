#include "AsyncLogWriterTests.h"

#include <chrono>

#include <Common/Log/AsyncLogWriter.h>
#include <Common/Log/LogLevel.h>
#include <Common/Log/AsyncLogWriterGuard.h>
#include <Common/Log/NullLogger.h>
#include <Common/Time/TimeTypes.h>

#include <Tests/DebugTestResult.h>
#include <Tests/TestHelpers.h>

#include "MemoryLogger.h"

namespace
{
	void RunStartStopTest(tests::DebugTestResult& result)
	{
		common::log::AsyncLogWriter logWriter;

		const common::log::AsyncLogWriter::StartResult startResult = logWriter.Start(1);
		const bool started = startResult.has_value();

		tests::Expect(result, started, "AsyncLogWriter: start succeeds");
		tests::Expect(result, logWriter.IsStarted(), "AsyncLogWriter: started after start");

		logWriter.Stop();

		tests::Expect(result, !logWriter.IsStarted(), "AsyncLogWriter: stopped after stop");
		tests::Expect(result, logWriter.GetPendingTaskCount() == 0, "AsyncLogWriter: pending count after stop");
	}

	void RunStartWithZeroWorkerFailsTest(tests::DebugTestResult& result)
	{
		common::log::AsyncLogWriter logWriter;

		const common::log::AsyncLogWriter::StartResult startResult = logWriter.Start(0);
		const bool started = startResult.has_value();

		tests::Expect(result, !started, "AsyncLogWriter: start with zero worker fails");
		tests::Expect(result, !logWriter.IsStarted(), "AsyncLogWriter: zero worker not started");
	}

	void RunStartTwiceFailsTest(tests::DebugTestResult& result)
	{
		common::log::AsyncLogWriter logWriter;

		const common::log::AsyncLogWriter::StartResult firstStartResult = logWriter.Start(1);
		const common::log::AsyncLogWriter::StartResult secondStartResult = logWriter.Start(1);

		const bool firstStarted = firstStartResult.has_value();
		const bool secondStarted = secondStartResult.has_value();

		tests::Expect(result, firstStarted, "AsyncLogWriter: first start succeeds");
		tests::Expect(result, !secondStarted, "AsyncLogWriter: second start fails");

		logWriter.Stop();
	}

	void RunLogBeforeStartFailsTest(tests::DebugTestResult& result)
	{
		common::log::AsyncLogWriter logWriter;

		const bool logged = logWriter.Info("message before start");

		tests::Expect(result, !logged, "AsyncLogWriter: log before start fails");
	}

	void RunLogAfterStopFailsTest(tests::DebugTestResult& result)
	{
		common::log::AsyncLogWriter logWriter;

		const common::log::AsyncLogWriter::StartResult startResult = logWriter.Start(1);
		const bool started = startResult.has_value();

		logWriter.Stop();

		const bool logged = logWriter.Info("message after stop");

		tests::Expect(result, started, "AsyncLogWriter: log after stop start succeeds");
		tests::Expect(result, !logged, "AsyncLogWriter: log after stop fails");
	}

	void RunLogEnqueueSucceedsTest(tests::DebugTestResult& result)
	{
		common::log::NullLogger nullLogger;
		common::log::AsyncLogWriter logWriter;
		logWriter.SetLogger(nullLogger);

		const common::log::AsyncLogWriter::StartResult startResult = logWriter.Start(1);
		const bool started = startResult.has_value();

		const bool logged = logWriter.Info("async log test");

		const bool drained = tests::WaitUntil(
			[&logWriter]()
			{
				return logWriter.GetPendingTaskCount() == 0;
			},
			common::time::Milliseconds(1000)
		);

		tests::Expect(result, started, "AsyncLogWriter: enqueue start succeeds");
		tests::Expect(result, logged, "AsyncLogWriter: enqueue succeeds");
		tests::Expect(result, drained, "AsyncLogWriter: queue drained");

		logWriter.Stop();
	}

	void RunMinimumLogLevelFiltersTest(tests::DebugTestResult& result)
	{
		common::log::NullLogger nullLogger;
		common::log::AsyncLogWriter logWriter;
		logWriter.SetLogger(nullLogger);

		const common::log::AsyncLogWriter::StartResult startResult = logWriter.Start(1);
		const bool started = startResult.has_value();

		logWriter.SetMinimumLogLevel(common::log::LogLevel::Warning);

		const bool infoLogged = logWriter.Info("filtered info log");
		const bool warningLogged = logWriter.Warning("visible warning log");

		tests::Expect(result, started, "AsyncLogWriter: filter start succeeds");
		tests::Expect(result, !infoLogged, "AsyncLogWriter: info filtered");
		tests::Expect(result, warningLogged, "AsyncLogWriter: warning accepted");

		logWriter.Stop();
	}

	void RunInjectedLoggerReceivesRecordTest(tests::DebugTestResult& result)
	{
		tests::log::MemoryLogger memoryLogger;
		common::log::AsyncLogWriter logWriter;
		logWriter.SetLogger(memoryLogger);

		const common::time::SystemTimePoint before = common::time::SystemClock::now();

		const common::log::AsyncLogWriter::StartResult startResult = logWriter.Start(1);
		const bool started = startResult.has_value();

		const bool logged = logWriter.Warning("captured warning log");

		const bool received = tests::WaitUntil(
			[&memoryLogger]()
			{
				return memoryLogger.GetLogRecordCount() == 1;
			},
			common::time::Milliseconds(1000)
		);

		const common::time::SystemTimePoint after = common::time::SystemClock::now();

		const std::vector<common::log::LogRecord> logRecordList = memoryLogger.GetLogRecords();

		tests::Expect(result, started, "AsyncLogWriter: injected logger start succeeds");
		tests::Expect(result, logged, "AsyncLogWriter: injected logger enqueue succeeds");
		tests::Expect(result, received, "AsyncLogWriter: injected logger receives record");

		if (!logRecordList.empty())
		{
			const common::log::LogRecord& logRecord = logRecordList.front();

			tests::Expect(
				result,
				logRecord.logLevel == common::log::LogLevel::Warning,
				"AsyncLogWriter: injected logger receives level"
			);

			tests::Expect(
				result,
				logRecord.message == "captured warning log",
				"AsyncLogWriter: injected logger receives message"
			);

			tests::Expect(
				result,
				logRecord.timestamp >= before && logRecord.timestamp <= after,
				"AsyncLogWriter: injected logger receives enqueue timestamp"
			);
		}

		logWriter.Stop();
	}

	void RunGuardStopsWriterTest(tests::DebugTestResult& result)
	{
		common::log::NullLogger nullLogger;
		common::log::AsyncLogWriter logWriter;
		logWriter.SetLogger(nullLogger);

		const common::log::AsyncLogWriter::StartResult startResult = logWriter.Start(1);

		tests::Expect(
			result,
			startResult.has_value(),
			"AsyncLogWriterGuard: start succeeds"
		);

		if (!startResult.has_value())
		{
			return;
		}

		{
			common::log::AsyncLogWriterGuard guard(logWriter);

			tests::Expect(
				result,
				guard.IsActive(),
				"AsyncLogWriterGuard: guard active"
			);

			tests::Expect(
				result,
				logWriter.IsStarted(),
				"AsyncLogWriterGuard: writer started"
			);
		}

		tests::Expect(
			result,
			!logWriter.IsStarted(),
			"AsyncLogWriterGuard: writer stopped on scope exit"
		);
	}
}

namespace tests::log
{
	tests::DebugTestResult RunAsyncLogWriterTests()
	{
		tests::DebugTestResult result{};

		RunStartStopTest(result);
		RunStartWithZeroWorkerFailsTest(result);
		RunStartTwiceFailsTest(result);
		RunLogBeforeStartFailsTest(result);
		RunLogAfterStopFailsTest(result);
		RunLogEnqueueSucceedsTest(result);
		RunMinimumLogLevelFiltersTest(result);
		RunInjectedLoggerReceivesRecordTest(result);
		RunGuardStopsWriterTest(result);

		return result;
	}
}