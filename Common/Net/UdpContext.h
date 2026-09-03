#pragma once

#include <WinSock2.h>

#include <array>
#include <cstddef>

#include "UdpTypes.h"

namespace common::net
{
	inline constexpr std::size_t udpBufferSize = 1200;

	using UdpBuffer = std::array<char, udpBufferSize>;

	// Overlapped UDP I/O - 완료될 때까지 필요한 작업 상태 보관 구조체
	struct UdpContext
	{
	public:
		OVERLAPPED overlapped{};
		WSABUF wsaBuffer{};
		UdpOperationType operationType = UdpOperationType::Recv;

	public:
		UdpContext() = default;
		~UdpContext() = default;

		UdpContext(const UdpContext& other) = delete;
		UdpContext& operator=(const UdpContext& other) = delete;

		UdpContext(UdpContext&& other) = delete;
		UdpContext& operator=(UdpContext&& other) = delete;
	};

	// 비동기 수신에 필요한 버퍼와 송신자 주소 정보 보관 구조체
	struct UdpRecvContext : public UdpContext
	{
	public:
		UdpBuffer buffer{};
		sockaddr_in remoteAddress{};
		int remoteAddressLength = static_cast<int>(sizeof(remoteAddress));
		DWORD flags = 0;

	public:
		UdpRecvContext();

	public:
		void Reset() noexcept;
	};

	// 비동기 송신이 완료될 때까지 전송 데이터와 목적지 주소 소유 구조체
	struct UdpSendContext : public UdpContext
	{
	public:
		UdpBuffer buffer{};
		sockaddr_in remoteAddress{};
		int remoteAddressLength = static_cast<int>(sizeof(remoteAddress));

	public:
		UdpSendContext();

	public:
		void Prepare(const sockaddr_in& address, const char* data, int size) noexcept;
	};

	// OVERLAPPED가 구조체의 첫 번째 멤버인지 확인용 컴파일 타임 단언문
	static_assert(offsetof(UdpContext, overlapped) == 0);
}