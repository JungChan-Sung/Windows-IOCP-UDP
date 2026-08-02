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
		const std::uint16_t networkPort = ::htons(port);

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
			== common::packet::AccountLoginResponseStatus
			::InvalidRequest,
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
			== common::packet::AccountLoginResponseStatus
			::ServerError,
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
		invalidRequestPacket.passwordHash = "password_hash";

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
		databaseFailurePacket.passwordHash = "password_hash";

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

		AccountLoginPacketHandler::ResponseTaskList
			responseTaskList
			= packetHandler.ExtractResponseTaskList(
				requestTime
				+ common::time::Seconds(1)
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
			packetHandler.GetPendingRequestCount() == 0,
			"AccountLoginPacketHandler: completed requests removed"
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

		tests::Expect(
			result,
			cachedResponseTaskList.size() == 1,
			"AccountLoginPacketHandler: cached response extracted"
		);

		RunInvalidRequestResponseTest(
			result,
			cachedResponseTaskList
		);

		const sockaddr_in stoppedProcessorAddress
			= MakeRemoteAddress(40002);

		common::packet::AccountLoginRequestPacket
			stoppedProcessorPacket{};

		stoppedProcessorPacket.requestId = 1003;
		stoppedProcessorPacket.loginName = "account";
		stoppedProcessorPacket.passwordHash = "password_hash";

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

		return result;
	}
}