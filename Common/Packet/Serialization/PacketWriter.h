#pragma once

#include <bit>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string_view>
#include <utility>

#include <Common/Packet/PacketBuffer.h>

namespace common::packet
{
	class PacketWriter
	{
	private:
		PacketBuffer buffer_;
		bool isValid_ = true;

	public:
		PacketWriter() = default;
		~PacketWriter() noexcept = default;

		PacketWriter(const PacketWriter&) = delete;
		PacketWriter& operator=(const PacketWriter&) = delete;

		PacketWriter(PacketWriter&&) noexcept = default;
		PacketWriter& operator=(PacketWriter&&) noexcept = default;

	public:
		void Reserve(std::size_t size)
		{
			buffer_.reserve(size);
		}

		void WriteUInt8(std::uint8_t value)
		{
			buffer_.push_back(static_cast<char>(value));
		}

		void WriteUInt16(std::uint16_t value)
		{
			WriteUInt8(static_cast<std::uint8_t>(value & 0x00FF));
			WriteUInt8(static_cast<std::uint8_t>((value >> 8) & 0x00FF));
		}

		void WriteUInt32(std::uint32_t value)
		{
			WriteUInt8(static_cast<std::uint8_t>(value & 0x000000FF));
			WriteUInt8(static_cast<std::uint8_t>((value >> 8) & 0x000000FF));
			WriteUInt8(static_cast<std::uint8_t>((value >> 16) & 0x000000FF));
			WriteUInt8(static_cast<std::uint8_t>((value >> 24) & 0x000000FF));
		}

		void WriteInt32(std::int32_t value)
		{
			WriteUInt32(std::bit_cast<std::uint32_t>(value));
		}

		void WriteFloat(float value)
		{
			WriteUInt32(std::bit_cast<std::uint32_t>(value));
		}

		void WriteString(std::string_view value)
		{
			if (value.size() > static_cast<std::size_t>(std::numeric_limits<std::uint16_t>::max()))
			{
				isValid_ = false;
				return;
			}

			WriteUInt16(static_cast<std::uint16_t>(value.size()));
			buffer_.insert(buffer_.end(), value.begin(), value.end());
		}

	public:
		[[nodiscard]] bool IsValid() const noexcept
		{
			return isValid_;
		}

		[[nodiscard]] const PacketBuffer& GetBuffer() const noexcept
		{
			return buffer_;
		}

		[[nodiscard]] PacketBuffer TakeBuffer() noexcept
		{
			return std::move(buffer_);
		}
	};
}