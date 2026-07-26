#pragma once

#include <bit>
#include <cstdint>
#include <string>

#include <Common/Packet/PacketConstants.h>

namespace common::packet
{
	class PacketReader
	{
	private:
		const char* data_ = nullptr;
		int size_ = 0;
		int offset_ = 0;

	public:
		PacketReader(const char* data, int size) noexcept
			: data_(data),
			size_(size)
		{}
		~PacketReader() noexcept = default;

		PacketReader(const PacketReader&) = delete;
		PacketReader& operator=(const PacketReader&) = delete;

		PacketReader(PacketReader&&) = delete;
		PacketReader& operator=(PacketReader&&) = delete;

	public:
		[[nodiscard]] bool ReadUInt8(std::uint8_t& value) noexcept
		{
			if (RemainingSize() < static_cast<int>(uint8WireSize))
			{
				return false;
			}

			value = static_cast<std::uint8_t>(static_cast<unsigned char>(data_[offset_]));
			++offset_;

			return true;
		}

		[[nodiscard]] bool ReadUInt16(std::uint16_t& value) noexcept
		{
			std::uint8_t byte0 = 0;
			std::uint8_t byte1 = 0;

			if (!ReadUInt8(byte0) || !ReadUInt8(byte1))
			{
				return false;
			}

			value = static_cast<std::uint16_t>(static_cast<std::uint16_t>(byte0) | static_cast<std::uint16_t>(static_cast<std::uint16_t>(byte1) << 8));

			return true;
		}

		[[nodiscard]] bool ReadUInt32(std::uint32_t& value) noexcept
		{
			std::uint8_t byte0 = 0;
			std::uint8_t byte1 = 0;
			std::uint8_t byte2 = 0;
			std::uint8_t byte3 = 0;

			if (!ReadUInt8(byte0) || !ReadUInt8(byte1) || !ReadUInt8(byte2) || !ReadUInt8(byte3))
			{
				return false;
			}

			value = static_cast<std::uint32_t>(byte0)
				| (static_cast<std::uint32_t>(byte1) << 8)
				| (static_cast<std::uint32_t>(byte2) << 16)
				| (static_cast<std::uint32_t>(byte3) << 24);

			return true;
		}

		[[nodiscard]] bool ReadUInt64(std::uint64_t& value) noexcept
		{
			std::uint32_t lowerValue = 0;
			std::uint32_t upperValue = 0;

			if (!ReadUInt32(lowerValue) || !ReadUInt32(upperValue))
			{
				return false;
			}

			value = static_cast<std::uint64_t>(lowerValue) | (static_cast<std::uint64_t>(upperValue) << 32);

			return true;
		}

		[[nodiscard]] bool ReadInt32(std::int32_t& value) noexcept
		{
			std::uint32_t rawValue = 0;
			if (!ReadUInt32(rawValue))
			{
				return false;
			}

			value = std::bit_cast<std::int32_t>(rawValue);

			return true;
		}

		[[nodiscard]] bool ReadInt64(std::int64_t& value) noexcept
		{
			std::uint64_t rawValue = 0;
			if (!ReadUInt64(rawValue))
			{
				return false;
			}

			value = std::bit_cast<std::int64_t>(rawValue);

			return true;
		}

		[[nodiscard]] bool ReadFloat(float& value) noexcept
		{
			std::uint32_t rawValue = 0;
			if (!ReadUInt32(rawValue))
			{
				return false;
			}

			value = std::bit_cast<float>(rawValue);

			return true;
		}

		[[nodiscard]] bool ReadString(std::string& value)
		{
			std::uint16_t stringSize = 0;
			if (!ReadUInt16(stringSize))
			{
				return false;
			}

			if (RemainingSize() < static_cast<int>(stringSize))
			{
				return false;
			}

			value.assign(data_ + offset_, static_cast<std::size_t>(stringSize));
			offset_ += static_cast<int>(stringSize);

			return true;
		}

	public:
		[[nodiscard]] int RemainingSize() const noexcept
		{
			return size_ - offset_;
		}

		[[nodiscard]] bool IsComplete() const noexcept
		{
			return offset_ == size_;
		}
	};
}