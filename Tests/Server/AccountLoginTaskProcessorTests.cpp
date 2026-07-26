#include "AccountLoginTaskProcessorTests.h"

#include <cstdint>
#include <variant>

#include <Persistence/Account/AccountValidation.h>
#include <Persistence/Core/DatabaseError.h>
#include <Persistence/Core/PersistenceRuntime.h>

#include <Server/Account/AccountLoginTaskProcessor.h>
#include <Server/Account/AccountService.h>

#include <Tests/DebugTestResult.h>

namespace
{
	[[nodiscard]] const ::server::account::AccountLoginCompletion* FindCompletionByTaskId(
		const ::server::account::AccountLoginTaskProcessor::CompletionList& completionList,
		::server::account::AccountLoginTaskId taskId
	)
	{
		for (const ::server::account::AccountLoginCompletion& completion : completionList)
		{
			if (completion.taskId == taskId)
			{
				return &completion;
			}
		}

		return nullptr;
	}

	void RunValidationFailureTest(
		tests::DebugTestResult& result,
		const ::server::account::AccountLoginTaskProcessor::CompletionList& completionList
	)
	{
		constexpr ::server::account::AccountLoginTaskId taskId = 100;

		const ::server::account::AccountLoginCompletion* completion
			= FindCompletionByTaskId(completionList, taskId);

		tests::Expect(
			result,
			completion != nullptr,
			"AccountLoginTaskProcessor: validation completion exists"
		);

		if (completion == nullptr)
		{
			return;
		}

		tests::Expect(
			result,
			!completion->loginResult.has_value(),
			"AccountLoginTaskProcessor: validation result fails"
		);

		if (completion->loginResult.has_value())
		{
			return;
		}

		const auto* validationError
			= std::get_if<persistence::account::AccountValidationError>(
				&completion->loginResult.error()
			);

		tests::Expect(
			result,
			validationError != nullptr,
			"AccountLoginTaskProcessor: validation error type"
		);

		tests::Expect(
			result,
			validationError != nullptr
			&& validationError->field
			== persistence::account::AccountField::LoginName
			&& validationError->failure
			== persistence::account::AccountValidationFailure::Empty,
			"AccountLoginTaskProcessor: validation error details"
		);
	}

	void RunDatabaseFailureTest(
		tests::DebugTestResult& result,
		const ::server::account::AccountLoginTaskProcessor::CompletionList& completionList
	)
	{
		constexpr ::server::account::AccountLoginTaskId taskId = 101;

		const ::server::account::AccountLoginCompletion* completion
			= FindCompletionByTaskId(completionList, taskId);

		tests::Expect(
			result,
			completion != nullptr,
			"AccountLoginTaskProcessor: database completion exists"
		);

		if (completion == nullptr)
		{
			return;
		}

		tests::Expect(
			result,
			!completion->loginResult.has_value(),
			"AccountLoginTaskProcessor: database result fails"
		);

		if (completion->loginResult.has_value())
		{
			return;
		}

		const auto* databaseError
			= std::get_if<persistence::core::DatabaseError>(
				&completion->loginResult.error()
			);

		tests::Expect(
			result,
			databaseError != nullptr,
			"AccountLoginTaskProcessor: database error type"
		);

		tests::Expect(
			result,
			databaseError != nullptr
			&& databaseError->failure
			== persistence::core::DatabaseFailure::ConnectionOpenFailed,
			"AccountLoginTaskProcessor: database error details"
		);
	}
}

namespace tests::server
{
	DebugTestResult RunAccountLoginTaskProcessorTests()
	{
		DebugTestResult result{};

		persistence::PersistenceRuntime persistenceRuntime;
		::server::account::AccountService accountService(persistenceRuntime);
		::server::account::AccountLoginTaskProcessor taskProcessor(accountService);

		const ::server::account::AccountLoginTaskProcessor::StartResult startResult
			= taskProcessor.Start(1);

		tests::Expect(
			result,
			startResult.has_value(),
			"AccountLoginTaskProcessor: start succeeds"
		);

		if (!startResult.has_value())
		{
			return result;
		}

		const bool validationTaskEnqueued = taskProcessor.Enqueue(
			::server::account::AccountLoginTask{
				.taskId = 100,
				.loginName = "",
				.passwordHash = "password_hash",
			}
			);

		tests::Expect(
			result,
			validationTaskEnqueued,
			"AccountLoginTaskProcessor: validation task enqueued"
		);

		const bool databaseTaskEnqueued = taskProcessor.Enqueue(
			::server::account::AccountLoginTask{
				.taskId = 101,
				.loginName = "account",
				.passwordHash = "password_hash",
			}
			);

		tests::Expect(
			result,
			databaseTaskEnqueued,
			"AccountLoginTaskProcessor: database task enqueued"
		);

		taskProcessor.StopAfterDrain();

		::server::account::AccountLoginTaskProcessor::CompletionList completionList
			= taskProcessor.ExtractCompletionList();

		tests::Expect(
			result,
			completionList.size() == 2,
			"AccountLoginTaskProcessor: two completions extracted"
		);

		RunValidationFailureTest(result, completionList);
		RunDatabaseFailureTest(result, completionList);

		const ::server::account::AccountLoginTaskProcessor::CompletionList emptyCompletionList
			= taskProcessor.ExtractCompletionList();

		tests::Expect(
			result,
			emptyCompletionList.empty(),
			"AccountLoginTaskProcessor: completion queue drained"
		);

		const bool enqueueAfterStopResult = taskProcessor.Enqueue(
			::server::account::AccountLoginTask{
				.taskId = 102,
				.loginName = "account",
				.passwordHash = "password_hash",
			}
			);

		tests::Expect(
			result,
			!enqueueAfterStopResult,
			"AccountLoginTaskProcessor: enqueue rejected after stop"
		);

		return result;
	}
}