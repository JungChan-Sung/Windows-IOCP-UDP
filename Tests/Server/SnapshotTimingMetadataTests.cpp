#include "SnapshotTimingMetadataTests.h"

#include <optional>
#include <vector>

#include <Common/Net/EndpointKey.h>
#include <Common/Packet/Game/GamePacket.h>
#include <Common/Packet/PacketSerialization.h>

#include <Server/Protocol/SnapshotBroadcastBuilder.h>
#include <Server/Protocol/SnapshotBroadcastContext.h>
#include <Server/Protocol/SnapshotBroadcastTask.h>

namespace
{
	void RunPlayerSnapshotTimingMetadataTest(tests::DebugTestResult& result)
	{
		server::protocol::SnapshotBroadcastContext context{};
		context.serverTick = 100;
		context.serverTickIntervalMilliseconds = 16;

		server::protocol::SnapshotRoomContext roomContext{};
		roomContext.roomId = 1;

		roomContext.peerContextList.push_back(
			server::protocol::SnapshotPeerContext{
				.endpointKey = common::net::EndpointKey{
					.address = 0x7F000001,
					.port = 10000,
				},
				.playerId = 1,
				.lastProcessedInputSequence = 10,
			}
			);

		roomContext.playerStateContextList.push_back(
			server::protocol::SnapshotPlayerStateContext{
				.playerId = 1,
				.x = 100.0F,
				.y = 200.0F,
				.hp = 3,
			}
			);

		context.roomContextList.push_back(roomContext);

		server::protocol::SnapshotBroadcastBuilder builder;

		const std::vector<server::protocol::PlayerSnapshotTask> taskList =
			builder.BuildPlayerSnapshotTasks(context);

		tests::Expect(
			result,
			taskList.size() == 1,
			"SnapshotTimingMetadata: one player snapshot task"
		);

		if (taskList.size() != 1)
		{
			return;
		}

		const common::packet::PlayerSnapshotPacket& snapshotPacket =
			taskList.front().snapshotPacket;

		tests::Expect(
			result,
			snapshotPacket.serverTickIntervalMilliseconds == 16,
			"SnapshotTimingMetadata: builder copies server tick interval"
		);

		const std::optional<common::packet::PacketBuffer> packetBuffer =
			common::packet::SerializePacket(snapshotPacket);

		tests::Expect(
			result,
			packetBuffer.has_value(),
			"SnapshotTimingMetadata: serialize player snapshot"
		);

		if (!packetBuffer.has_value())
		{
			return;
		}

		const std::optional<common::packet::PlayerSnapshotPacket> deserializedPacket =
			common::packet::DeserializePacket<common::packet::PlayerSnapshotPacket>(
				packetBuffer->data(),
				static_cast<int>(packetBuffer->size())
			);

		tests::Expect(
			result,
			deserializedPacket.has_value(),
			"SnapshotTimingMetadata: deserialize player snapshot"
		);

		if (!deserializedPacket.has_value())
		{
			return;
		}

		tests::Expect(
			result,
			deserializedPacket->serverTick == 100,
			"SnapshotTimingMetadata: server tick roundtrip"
		);

		tests::Expect(
			result,
			deserializedPacket->serverTickIntervalMilliseconds == 16,
			"SnapshotTimingMetadata: server tick interval roundtrip"
		);
	}
}

namespace tests::server
{
	DebugTestResult RunSnapshotTimingMetadataTests()
	{
		DebugTestResult result{};

		RunPlayerSnapshotTimingMetadataTest(result);

		return result;
	}
}