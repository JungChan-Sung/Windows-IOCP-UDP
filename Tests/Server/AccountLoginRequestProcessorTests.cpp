#include "AccountLoginRequestProcessorTests.h"

#include <WinSock2.h>

#include <cstddef>

#include <Common/Packet/Account/AccountPacket.h>

#include <Persistence/Core/PersistenceRuntime.h>

#include <Server/Account/AccountService.h>
#include <Server/Net/AccountLoginRequestProcessor.h>

#include <Tests/DebugTestResult.h>

namespace tests::server
{
	DebugTestResult RunAccountLoginRequestProcessorTests()
	{
		DebugTestResult result{};

		persistence::PersistenceRuntime persistenceRuntime;
		::server::account::AccountService accountService(persistenceRuntime);
		::server::net::AccountLoginRequestProcessor processor(accountService);

		const ::server::net::AccountLoginRequestProcessor::StartResult startResult = processor.Start();
		tests::Expect(
			result,
			startResult.has_value(),
			"AccountLoginRequestProcessor: start succeeds"
		);

		if (!startResult.has_value())
		{
			return result;
		}

		sockaddr_in remoteAddress{};
		remoteAddress.sin_family = AF_INET;
		remoteAddress.sin_port = ::htons(40000);
		remoteAddress.sin_addr.S_un.S_addr = ::htonl(0x7F000001);

		common::packet::AccountLoginRequestPacket invalidRequestPacket{};
		invalidRequestPacket.loginName = "";
		invalidRequestPacket.passwordHash = "password_hash";

		tests::Expect(
			result,
			processor.Enqueue(remoteAddress, invalidRequestPacket),
			"AccountLoginRequestProcessor: invalid request enqueued"
		);

		common::packet::AccountLoginRequestPacket databaseFailurePacket{};
		databaseFailurePacket.loginName = "account";
		databaseFailurePacket.passwordHash = "password_hash";

		tests::Expect(
			result,
			processor.Enqueue(remoteAddress, databaseFailurePacket),
			"AccountLoginRequestProcessor: database request enqueued"
		);

		processor.StopAfterDrain();

		::server::net::AccountLoginRequestProcessor::ResponseTaskList responseTaskList
			= processor.ExtractResponseTaskList();

		tests::Expect(
			result,
			responseTaskList.size() == 2,
			"AccountLoginRequestProcessor: two responses completed"
		);

		if (responseTaskList.size() == 2)
		{
			tests::Expect(
				result,
				responseTaskList[0].responsePacket.status
				== common::packet::AccountLoginResponseStatus::InvalidRequest,
				"AccountLoginRequestProcessor: validation response mapped"
			);

			tests::Expect(
				result,
				responseTaskList[1].responsePacket.status
				== common::packet::AccountLoginResponseStatus::ServerError,
				"AccountLoginRequestProcessor: database response mapped"
			);

			tests::Expect(
				result,
				responseTaskList[0].remoteAddress.sin_port
				== remoteAddress.sin_port,
				"AccountLoginRequestProcessor: endpoint preserved"
			);
		}

		return result;
	}
}