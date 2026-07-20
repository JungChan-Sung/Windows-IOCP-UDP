#include "PersistenceRuntimeIntegrationTests.h"

#include <Windows.h>

#include <expected>
#include <future>
#include <iostream>
#include <latch>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <Persistence/Core/DatabaseError.h>
#include <Persistence/Core/PersistenceRuntime.h>

#include <Tests/DebugTestResult.h>

namespace
{
	inline constexpr std::string_view
		databaseConnectionStringEnvironmentName
		= "WINDOWS_IOCP_UDP_TEST_DB_CONNECTION_STRING";

	using WorkerResult = std::expected<
		void,
		::persistence::core::DatabaseError
	>;

	[[nodiscard]] std::optional<std::string>
		ReadDatabaseConnectionString()
	{
		const DWORD requiredSize = ::GetEnvironmentVariableA(
			databaseConnectionStringEnvironmentName.data(),
			nullptr,
			0
		);

		if (requiredSize == 0)
		{
			return std::nullopt;
		}

		std::string value(requiredSize, '\0');

		const DWORD copiedSize = ::GetEnvironmentVariableA(
			databaseConnectionStringEnvironmentName.data(),
			value.data(),
			requiredSize
		);

		if (copiedSize == 0 || copiedSize >= requiredSize)
		{
			return std::nullopt;
		}

		value.resize(copiedSize);

		return value;
	}

	void AddDatabaseFailure(
		tests::DebugTestResult& result,
		std::string_view operationName,
		const ::persistence::core::DatabaseError& error
	)
	{
		std::string message(operationName);
		message += ": ";
		message += ::persistence::core::ToString(error);

		result.AddFailed(message);
	}

	[[nodiscard]] WorkerResult RunConcurrentExistsWorker(
		::persistence::PersistenceRuntime& persistenceRuntime,
		std::latch& startSignal,
		std::size_t operationCount
	)
	{
		startSignal.wait();

		for (std::size_t index = 0;
			index < operationCount;
			++index)
		{
			const ::persistence::PersistenceRuntime
				::ExistsAccountResult existsResult
				= persistenceRuntime.ExistsByLoginName(
					"persistence_runtime_concurrent_missing_account"
				);

			if (!existsResult.has_value())
			{
				return std::unexpected(
					existsResult.error()
				);
			}
		}

		return {};
	}
}

namespace tests::persistence
{
	DebugTestResult RunPersistenceRuntimeIntegrationTests()
	{
		DebugTestResult result{};

		const std::optional<std::string> connectionString
			= ReadDatabaseConnectionString();

		if (!connectionString.has_value())
		{
			std::cout
				<< "[PersistenceRuntimeIntegration] Skipped: "
				<< databaseConnectionStringEnvironmentName
				<< " is not set.\n";

			return result;
		}

		::persistence::PersistenceRuntime persistenceRuntime;

		const ::persistence::PersistenceRuntime::StartResult
			startResult
			= persistenceRuntime.Start(
				::persistence::PersistenceRuntimeStartConfig{
					.enabled = true,
					.connectionString = *connectionString,
					.connectionTimeoutSeconds = 5,
				}
				);

		if (!startResult.has_value())
		{
			AddDatabaseFailure(
				result,
				"PersistenceRuntimeIntegration: start failed",
				startResult.error()
			);

			return result;
		}

		tests::Expect(
			result,
			persistenceRuntime.IsEnabled(),
			"PersistenceRuntimeIntegration: enabled after start"
		);

		tests::Expect(
			result,
			persistenceRuntime.IsStarted(),
			"PersistenceRuntimeIntegration: started before "
			"concurrent access"
		);

		constexpr std::size_t workerCount = 8;
		constexpr std::size_t operationCountPerWorker = 8;

		std::latch startSignal{ 1 };

		std::vector<std::future<WorkerResult>> workers;
		workers.reserve(workerCount);

		for (std::size_t workerIndex = 0;
			workerIndex < workerCount;
			++workerIndex)
		{
			workers.push_back(
				std::async(
					std::launch::async,
					[
						&persistenceRuntime,
						&startSignal
					]()
					{
						return RunConcurrentExistsWorker(
							persistenceRuntime,
							startSignal,
							operationCountPerWorker
						);
					}
				)
			);
		}

		startSignal.count_down();

		std::optional<::persistence::core::DatabaseError>
			firstWorkerError;

		for (std::future<WorkerResult>& worker : workers)
		{
			WorkerResult workerResult = worker.get();

			if (!workerResult.has_value()
				&& !firstWorkerError.has_value())
			{
				firstWorkerError = workerResult.error();
			}
		}

		std::string concurrentAccessMessage
			= "PersistenceRuntimeIntegration: concurrent "
			"database operations succeed";

		if (firstWorkerError.has_value())
		{
			concurrentAccessMessage += ": ";
			concurrentAccessMessage
				+= ::persistence::core::ToString(
					*firstWorkerError
				);
		}

		tests::Expect(
			result,
			!firstWorkerError.has_value(),
			concurrentAccessMessage
		);

		tests::Expect(
			result,
			persistenceRuntime.IsStarted(),
			"PersistenceRuntimeIntegration: remains started "
			"after concurrent access"
		);

		persistenceRuntime.Stop();

		tests::Expect(
			result,
			!persistenceRuntime.IsEnabled(),
			"PersistenceRuntimeIntegration: disabled after stop"
		);

		tests::Expect(
			result,
			!persistenceRuntime.IsStarted(),
			"PersistenceRuntimeIntegration: stopped after stop"
		);

		return result;
	}
}