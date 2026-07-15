#include "PersistenceRuntimeTests.h"

#include <Persistence/Core/PersistenceRuntime.h>

#include <Tests/DebugTestResult.h>

namespace
{
	void RunRejectOperationBeforeStartTest(
		tests::DebugTestResult& result
	)
	{
		::persistence::PersistenceRuntime persistenceRuntime;

		const ::persistence::PersistenceRuntime::ExistsAccountResult
			existsResult
			= persistenceRuntime.ExistsByLoginName(
				"not_started_test"
			);

		tests::Expect(
			result,
			!existsResult.has_value(),
			"PersistenceRuntime: reject operation before start"
		);

		tests::Expect(
			result,
			!persistenceRuntime.IsEnabled(),
			"PersistenceRuntime: disabled before start"
		);

		tests::Expect(
			result,
			!persistenceRuntime.IsStarted(),
			"PersistenceRuntime: not started initially"
		);
	}

	void RunDisabledStartTest(
		tests::DebugTestResult& result
	)
	{
		::persistence::PersistenceRuntime persistenceRuntime;

		const ::persistence::PersistenceRuntime::StartResult startResult
			= persistenceRuntime.Start(
				::persistence::PersistenceRuntimeStartConfig{
					.enabled = false,
				}
				);

		tests::Expect(
			result,
			startResult.has_value(),
			"PersistenceRuntime: disabled start succeeds"
		);

		tests::Expect(
			result,
			!persistenceRuntime.IsEnabled(),
			"PersistenceRuntime: remains disabled"
		);

		tests::Expect(
			result,
			!persistenceRuntime.IsStarted(),
			"PersistenceRuntime: disabled start does not open connection"
		);
	}

	void RunFailedStartResetsStateTest(
		tests::DebugTestResult& result
	)
	{
		::persistence::PersistenceRuntime persistenceRuntime;

		const ::persistence::PersistenceRuntime::StartResult startResult
			= persistenceRuntime.Start(
				::persistence::PersistenceRuntimeStartConfig{
					.enabled = true,
					.connectionString = "",
					.connectionTimeoutSeconds = 5,
				}
				);

		tests::Expect(
			result,
			!startResult.has_value(),
			"PersistenceRuntime: reject empty connection string"
		);

		tests::Expect(
			result,
			!persistenceRuntime.IsEnabled(),
			"PersistenceRuntime: failed start resets enabled state"
		);

		tests::Expect(
			result,
			!persistenceRuntime.IsStarted(),
			"PersistenceRuntime: failed start leaves runtime stopped"
		);
	}
}

namespace tests::persistence
{
	DebugTestResult RunPersistenceRuntimeTests()
	{
		DebugTestResult result{};

		RunRejectOperationBeforeStartTest(result);
		RunDisabledStartTest(result);
		RunFailedStartResetsStateTest(result);

		return result;
	}
}