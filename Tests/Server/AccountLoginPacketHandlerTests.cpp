#include "AccountLoginPacketHandlerTests.h"

#include <WinSock2.h>

#include <cstdint>

#include <Common/Packet/Account/AccountPacket.h>
#include <Common/Time/TimeTypes.h>

#include <Persistence/Core/PersistenceRuntime.h>

#include <Server/Account/AccountLoginTaskProcessor.h>
#include <Server/Account/AccountService.h>
#include <Server/Net/AccountLoginPacketHandler.h>

#include <Tests/DebugTestResult.h>

namespace
{
	using AccountLoginPacketHandler
		= ::server::net::AccountLoginPacketHandler;

	using ResponseStatus
		= common::packet::AccountLoginResponseStatus;

	[[nodiscard]] sockaddr_in MakeRemoteAddress(
		std::uint16_t port
	) noexcept
	{
		sockaddr_in remoteAddress{};
		remoteAddress.sin_family = AF_INET;
		remoteAddress.sin_port = ::htons(port);
		remoteAddress.sin_addr.S_un.S_addr
			= ::htonl(0x7F000001);

		return remoteAddress;
	}

	[[nodiscard]]
	const AccountLoginPacketHandler::ResponseTask*
		FindResponseTaskByPort(
			const AccountLoginPacketHandler::ResponseTaskList&
			responseTaskList,
			std::uint16_t port
		)
	{
		const std::uint16_t networkPort
			= ::htons(port);

		for (const AccountLoginPacketHandler::ResponseTask&
			responseTask : responseTaskList)
		{
			if (responseTask.remoteAddress.sin_port
				== networkPort)
			{
				return &responseTask;
			}
		}

		return nullptr;
	}

	[[nodiscard]]
	const AccountLoginPacketHandler::ResponseTask*
		FindResponseTaskByRequestId(
			const AccountLoginPacketHandler::ResponseTaskList& responseTaskList,
			common::packet::AccountLoginRequestId requestId
		)
	{
		for (const AccountLoginPacketHandler::ResponseTask& responseTask
			: responseTaskList)
		{
			if (responseTask.responsePacket.requestId == requestId)
			{
				return &responseTask;
			}
		}

		return nullptr;
	}

	void RunInvalidRequestResponseTest(
		tests::DebugTestResult& result,
		const AccountLoginPacketHandler::ResponseTaskList&
		responseTaskList
	)
	{
		const AccountLoginPacketHandler::ResponseTask*
			responseTask
			= FindResponseTaskByPort(
				responseTaskList,
				40000
			);

		tests::Expect(
			result,
			responseTask != nullptr,
			"AccountLoginPacketHandler: invalid request response exists"
		);

		if (responseTask == nullptr)
		{
			return;
		}

		tests::Expect(
			result,
			responseTask->responsePacket.requestId == 1001,
			"AccountLoginPacketHandler: invalid request id preserved"
		);

		tests::Expect(
			result,
			responseTask->remoteAddress.sin_family == AF_INET,
			"AccountLoginPacketHandler: invalid request address family preserved"
		);

		tests::Expect(
			result,
			responseTask->remoteAddress.sin_addr.S_un.S_addr
			== ::htonl(0x7F000001),
			"AccountLoginPacketHandler: invalid request IP preserved"
		);

		tests::Expect(
			result,
			responseTask->responsePacket.status
			== ResponseStatus::InvalidRequest,
			"AccountLoginPacketHandler: validation failure mapped"
		);

		tests::Expect(
			result,
			responseTask->responsePacket.accountId == 0
			&& responseTask
			->responsePacket
			.nickname
			.empty(),
			"AccountLoginPacketHandler: invalid request account data cleared"
		);

		tests::Expect(result, responseTask->persistentPlayerId == 0, "AccountLoginPacketHandler: invalid request persistent player id cleared");
	}

	void RunDatabaseFailureResponseTest(
		tests::DebugTestResult& result,
		const AccountLoginPacketHandler::ResponseTaskList&
		responseTaskList
	)
	{
		const AccountLoginPacketHandler::ResponseTask*
			responseTask
			= FindResponseTaskByPort(
				responseTaskList,
				40001
			);

		tests::Expect(
			result,
			responseTask != nullptr,
			"AccountLoginPacketHandler: database response exists"
		);

		if (responseTask == nullptr)
		{
			return;
		}

		tests::Expect(
			result,
			responseTask->responsePacket.requestId == 1002,
			"AccountLoginPacketHandler: database request id preserved"
		);

		tests::Expect(
			result,
			responseTask->responsePacket.status
			== ResponseStatus::ServerError,
			"AccountLoginPacketHandler: database failure mapped"
		);

		tests::Expect(
			result,
			responseTask->responsePacket.accountId == 0
			&& responseTask
			->responsePacket
			.nickname
			.empty(),
			"AccountLoginPacketHandler: database failure account data cleared"
		);

		tests::Expect(result, responseTask->persistentPlayerId == 0, "AccountLoginPacketHandler: database failure persistent player id cleared");
	}

	void FinalizeResponseTaskList(
		tests::DebugTestResult& result,
		AccountLoginPacketHandler& packetHandler,
		const AccountLoginPacketHandler::ResponseTaskList&
		responseTaskList,
		common::time::TimePoint currentTime
	)
	{
		for (const AccountLoginPacketHandler::ResponseTask&
			responseTask : responseTaskList)
		{
			tests::Expect(
				result,
				responseTask.taskId
				!= AccountLoginPacketHandler::invalidTaskId,
				"AccountLoginPacketHandler: new response has task id"
			);

			common::packet::AccountLoginResponsePacket
				finalResponsePacket
				= responseTask.responsePacket;

			/*
			 * 서버의 입장 정책이 최종 응답을 변경하는 상황을 재현한다.
			 * 이 변경된 응답이 캐시에 저장되어야 한다.
			 */
			if (responseTask.remoteAddress.sin_port
				== ::htons(40000))
			{
				finalResponsePacket.status
					= ResponseStatus::AlreadyLoggedIn;

				finalResponsePacket.accountId = 0;
				finalResponsePacket.nickname.clear();
			}

			const bool finalized
				= packetHandler.FinalizeResponse(
					responseTask.taskId,
					finalResponsePacket,
					currentTime
				);

			tests::Expect(
				result,
				finalized,
				"AccountLoginPacketHandler: response finalized"
			);
		}
	}

	void RunCachedFinalResponseTest(
		tests::DebugTestResult& result,
		const AccountLoginPacketHandler::ResponseTaskList&
		responseTaskList
	)
	{
		tests::Expect(
			result,
			responseTaskList.size() == 1,
			"AccountLoginPacketHandler: cached response extracted"
		);

		if (responseTaskList.size() != 1)
		{
			return;
		}

		const AccountLoginPacketHandler::ResponseTask&
			responseTask = responseTaskList.front();

		tests::Expect(
			result,
			responseTask.taskId
			== AccountLoginPacketHandler::invalidTaskId,
			"AccountLoginPacketHandler: cached response has no task id"
		);

		tests::Expect(
			result,
			responseTask.remoteAddress.sin_port
			== ::htons(40000),
			"AccountLoginPacketHandler: cached response address preserved"
		);

		tests::Expect(
			result,
			responseTask.responsePacket.requestId == 1001,
			"AccountLoginPacketHandler: cached request id preserved"
		);

		tests::Expect(
			result,
			responseTask.responsePacket.status
			== ResponseStatus::AlreadyLoggedIn,
			"AccountLoginPacketHandler: finalized status cached"
		);

		tests::Expect(
			result,
			responseTask.responsePacket.accountId == 0
			&& responseTask
			.responsePacket
			.nickname
			.empty(),
			"AccountLoginPacketHandler: cached failure account data cleared"
		);
	}

	void RunLatestRequestSelectionTest(
		tests::DebugTestResult& result
	)
	{
		persistence::PersistenceRuntime persistenceRuntime;

		::server::account::AccountService accountService(
			persistenceRuntime
		);

		::server::account::AccountLoginTaskProcessor taskProcessor(
			accountService
		);

		AccountLoginPacketHandler packetHandler(
			taskProcessor
		);

		const auto startResult = taskProcessor.Start(1);

		tests::Expect(
			result,
			startResult.has_value(),
			"AccountLoginPacketHandler: latest request processor starts"
		);

		if (!startResult.has_value())
		{
			return;
		}

		const common::time::TimePoint requestTime{};
		const sockaddr_in remoteAddress = MakeRemoteAddress(41000);

		common::packet::AccountLoginRequestPacket firstPacket{};
		firstPacket.requestId = 2001;
		firstPacket.loginName = "";
		firstPacket.passwordHash = "password_hash";

		common::packet::AccountLoginRequestPacket secondPacket{};
		secondPacket.requestId = 2002;
		secondPacket.loginName = "";
		secondPacket.passwordHash = "password_hash";

		const AccountLoginPacketHandler::EnqueueStatus firstStatus
			= packetHandler.Enqueue(
				remoteAddress,
				firstPacket,
				requestTime
			);

		tests::Expect(
			result,
			firstStatus
			== AccountLoginPacketHandler::EnqueueStatus::Enqueued,
			"AccountLoginPacketHandler: first overlapping request enqueued"
		);

		const AccountLoginPacketHandler::EnqueueStatus secondStatus
			= packetHandler.Enqueue(
				remoteAddress,
				secondPacket,
				requestTime
			);

		tests::Expect(
			result,
			secondStatus
			== AccountLoginPacketHandler::EnqueueStatus::Enqueued,
			"AccountLoginPacketHandler: second overlapping request enqueued"
		);

		tests::Expect(
			result,
			packetHandler.GetPendingRequestCount() == 2,
			"AccountLoginPacketHandler: overlapping requests remain pending"
		);

		taskProcessor.StopAfterDrain();

		const common::time::TimePoint completionTime
			= requestTime + common::time::Seconds(1);

		const AccountLoginPacketHandler::ResponseTaskList responseTaskList
			= packetHandler.ExtractResponseTaskList(completionTime);

		tests::Expect(
			result,
			responseTaskList.size() == 2,
			"AccountLoginPacketHandler: overlapping responses extracted"
		);

		const AccountLoginPacketHandler::ResponseTask* firstResponseTask
			= FindResponseTaskByRequestId(
				responseTaskList,
				firstPacket.requestId
			);

		const AccountLoginPacketHandler::ResponseTask* secondResponseTask
			= FindResponseTaskByRequestId(
				responseTaskList,
				secondPacket.requestId
			);

		tests::Expect(
			result,
			firstResponseTask != nullptr,
			"AccountLoginPacketHandler: first overlapping response exists"
		);

		tests::Expect(
			result,
			secondResponseTask != nullptr,
			"AccountLoginPacketHandler: second overlapping response exists"
		);

		if (firstResponseTask != nullptr)
		{
			tests::Expect(
				result,
				!firstResponseTask->isLatestRequest,
				"AccountLoginPacketHandler: older request marked stale"
			);
		}

		if (secondResponseTask != nullptr)
		{
			tests::Expect(
				result,
				secondResponseTask->isLatestRequest,
				"AccountLoginPacketHandler: newest request marked latest"
			);
		}

		for (const AccountLoginPacketHandler::ResponseTask& responseTask
			: responseTaskList)
		{
			const bool finalized = packetHandler.FinalizeResponse(
				responseTask.taskId,
				responseTask.responsePacket,
				completionTime
			);

			tests::Expect(
				result,
				finalized,
				"AccountLoginPacketHandler: overlapping response finalized"
			);
		}

		tests::Expect(
			result,
			packetHandler.GetPendingRequestCount() == 0,
			"AccountLoginPacketHandler: overlapping requests removed"
		);
	}
}

namespace tests::server
{
	DebugTestResult RunAccountLoginPacketHandlerTests()
	{
		DebugTestResult result{};

		persistence::PersistenceRuntime persistenceRuntime;

		::server::account::AccountService accountService(
			persistenceRuntime
		);

		::server::account::AccountLoginTaskProcessor
			taskProcessor(accountService);

		AccountLoginPacketHandler packetHandler(
			taskProcessor
		);

		const ::server::account::AccountLoginTaskProcessor
			::StartResult startResult
			= taskProcessor.Start(1);

		tests::Expect(
			result,
			startResult.has_value(),
			"AccountLoginPacketHandler: task processor starts"
		);

		if (!startResult.has_value())
		{
			return result;
		}

		const common::time::TimePoint requestTime{};

		const sockaddr_in invalidRequestAddress
			= MakeRemoteAddress(40000);

		common::packet::AccountLoginRequestPacket
			invalidRequestPacket{};

		invalidRequestPacket.requestId = 1001;
		invalidRequestPacket.loginName = "";
		invalidRequestPacket.passwordHash
			= "password_hash";

		const AccountLoginPacketHandler::EnqueueStatus
			invalidRequestStatus
			= packetHandler.Enqueue(
				invalidRequestAddress,
				invalidRequestPacket,
				requestTime
			);

		tests::Expect(
			result,
			invalidRequestStatus
			== AccountLoginPacketHandler::EnqueueStatus
			::Enqueued,
			"AccountLoginPacketHandler: invalid request enqueued"
		);

		const AccountLoginPacketHandler::EnqueueStatus
			duplicatePendingStatus
			= packetHandler.Enqueue(
				invalidRequestAddress,
				invalidRequestPacket,
				requestTime
			);

		tests::Expect(
			result,
			duplicatePendingStatus
			== AccountLoginPacketHandler::EnqueueStatus
			::DuplicatePending,
			"AccountLoginPacketHandler: pending duplicate detected"
		);

		const sockaddr_in databaseFailureAddress
			= MakeRemoteAddress(40001);

		common::packet::AccountLoginRequestPacket
			databaseFailurePacket{};

		databaseFailurePacket.requestId = 1002;
		databaseFailurePacket.loginName = "account";
		databaseFailurePacket.passwordHash
			= "password_hash";

		const AccountLoginPacketHandler::EnqueueStatus
			databaseRequestStatus
			= packetHandler.Enqueue(
				databaseFailureAddress,
				databaseFailurePacket,
				requestTime
			);

		tests::Expect(
			result,
			databaseRequestStatus
			== AccountLoginPacketHandler::EnqueueStatus
			::Enqueued,
			"AccountLoginPacketHandler: database request enqueued"
		);

		tests::Expect(
			result,
			packetHandler.GetPendingRequestCount() == 2,
			"AccountLoginPacketHandler: duplicate does not add pending request"
		);

		taskProcessor.StopAfterDrain();

		const common::time::TimePoint completionTime
			= requestTime
			+ common::time::Seconds(1);

		AccountLoginPacketHandler::ResponseTaskList
			responseTaskList
			= packetHandler.ExtractResponseTaskList(
				completionTime
			);

		tests::Expect(
			result,
			responseTaskList.size() == 2,
			"AccountLoginPacketHandler: two responses extracted"
		);

		RunInvalidRequestResponseTest(
			result,
			responseTaskList
		);

		RunDatabaseFailureResponseTest(
			result,
			responseTaskList
		);

		tests::Expect(
			result,
			packetHandler.GetPendingRequestCount() == 2,
			"AccountLoginPacketHandler: extracted responses await finalization"
		);

		FinalizeResponseTaskList(
			result,
			packetHandler,
			responseTaskList,
			completionTime
		);

		tests::Expect(
			result,
			packetHandler.GetPendingRequestCount() == 0,
			"AccountLoginPacketHandler: finalized requests removed"
		);

		const AccountLoginPacketHandler::EnqueueStatus
			cachedResponseStatus
			= packetHandler.Enqueue(
				invalidRequestAddress,
				invalidRequestPacket,
				requestTime
				+ common::time::Seconds(2)
			);

		tests::Expect(
			result,
			cachedResponseStatus
			== AccountLoginPacketHandler::EnqueueStatus
			::CachedResponseQueued,
			"AccountLoginPacketHandler: completed duplicate uses cache"
		);

		tests::Expect(
			result,
			packetHandler.GetPendingRequestCount() == 0,
			"AccountLoginPacketHandler: cached duplicate creates no task"
		);

		AccountLoginPacketHandler::ResponseTaskList
			cachedResponseTaskList
			= packetHandler.ExtractResponseTaskList(
				requestTime
				+ common::time::Seconds(2)
			);

		RunCachedFinalResponseTest(
			result,
			cachedResponseTaskList
		);

		const sockaddr_in stoppedProcessorAddress
			= MakeRemoteAddress(40002);

		common::packet::AccountLoginRequestPacket
			stoppedProcessorPacket{};

		stoppedProcessorPacket.requestId = 1003;
		stoppedProcessorPacket.loginName = "account";
		stoppedProcessorPacket.passwordHash
			= "password_hash";

		const AccountLoginPacketHandler::EnqueueStatus
			enqueueAfterStopStatus
			= packetHandler.Enqueue(
				stoppedProcessorAddress,
				stoppedProcessorPacket,
				requestTime
				+ common::time::Seconds(3)
			);

		tests::Expect(
			result,
			enqueueAfterStopStatus
			== AccountLoginPacketHandler::EnqueueStatus
			::TaskEnqueueFailed,
			"AccountLoginPacketHandler: enqueue rejected after processor stop"
		);

		tests::Expect(
			result,
			packetHandler.GetPendingRequestCount() == 0,
			"AccountLoginPacketHandler: failed enqueue rolls back request"
		);

		const AccountLoginPacketHandler::ResponseTaskList
			emptyResponseTaskList
			= packetHandler.ExtractResponseTaskList(
				requestTime
				+ common::time::Seconds(3)
			);

		tests::Expect(
			result,
			emptyResponseTaskList.empty(),
			"AccountLoginPacketHandler: response queue drained"
		);

		packetHandler.Clear();

		tests::Expect(
			result,
			packetHandler.GetPendingRequestCount() == 0,
			"AccountLoginPacketHandler: clear removes pending requests"
		);

		RunLatestRequestSelectionTest(result);

		return result;
	}
}