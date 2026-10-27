#pragma once

#include "wire.h"

#include <cstddef>
#include <span>
#include <utility>
#include <vector>

namespace hb::net
{
	// Appends whole wire structs to a byte buffer it owns; take() hands the buffer over.
	class span_writer
	{
	public:
		template <wire_type T>
		void write(const T& value)
		{
			append(std::as_bytes(std::span<const T, 1>(&value, 1)));
		}

		template <wire_type T>
		void write_array(std::span<const T> values)
		{
			append(std::as_bytes(values));
		}

		std::span<const std::byte> bytes() const
		{
			return m_bytes;
		}

		std::size_t size() const
		{
			return m_bytes.size();
		}

		std::vector<std::byte> take()
		{
			return std::exchange(m_bytes, {});
		}

	private:
		void append(std::span<const std::byte> bytes)
		{
			m_bytes.insert(m_bytes.end(), bytes.begin(), bytes.end());
		}

		std::vector<std::byte> m_bytes;
	};
}
