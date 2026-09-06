#pragma once

#include <cstdint>

#include <Common/Time/TimeTypes.h>

namespace common::net
{
	// UDP 송신 경로에 적용할 손실·중복·지연·순서 뒤바뀜 시뮬레이션 정책을 정의하는 클래스
	struct UdpFaultSimulationConfig
	{
	public:
		bool enabled = false;

		float dropRate = 0.0F;
		float duplicateRate = 0.0F;
		float reorderRate = 0.0F;

		time::Milliseconds minDelay{};
		time::Milliseconds maxDelay{};
		time::Milliseconds reorderDelay = time::Milliseconds(100);	// Reorder가 선택된 패킷에 추가 지연을 부여해 뒤 패킷이 먼저 전송될 가능성을 만듬

		std::uint32_t randomSeed = 5489;	// 동일한 Fault 패턴을 재현할 수 있도록 난수 생성기의 seed를 고정
	};
}