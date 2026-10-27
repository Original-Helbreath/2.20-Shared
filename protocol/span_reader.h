#pragma once

#include "wire.h"

#include <cstddef>
#include <cstring>
#include <span>
#include <vector>

namespace hb::net
{
	// Reads whole wire structs out of a byte span, front to back. Every read either takes exactly
	// sizeof(T) bytes per value or takes nothing and returns false (short input is recoverable).
	class span_reader
	{
	public:
		explicit span_reader(std::span<const std::byte> bytes)
			: m_bytes(bytes)
		{
		}

		template <wire_type T>
		[[nodiscard]] bool read(T& out)
		{
			if (m_bytes.size() < sizeof(T))
				return false;
			std::memcpy(&out, m_bytes.data(), sizeof(T));
			m_bytes = m_bytes.subspan(sizeof(T));
			return true;
		}

		// A counted tail of records. False, with out cleared and nothing consumed, if fewer than
		// count records remain.
		template <wire_type T>
		[[nodiscard]] bool read_array(std::size_t count, std::vector<T>& out)
		{
			out.clear();
			if (count > m_bytes.size() / sizeof(T))
				return false;
			const std::size_t size = count * sizeof(T);
			out.resize(count);
			if (size != 0)
				std::memcpy(out.data(), m_bytes.data(), size);
			m_bytes = m_bytes.subspan(size);
			return true;
		}

		std::size_t remaining() const
		{
			return m_bytes.size();
		}

		bool at_end() const
		{
			return m_bytes.empty();
		}

	private:
		std::span<const std::byte> m_bytes;
	};

	// A fixed-size body: exactly sizeof(T) bytes, no more and no less (plan sect. 5.2).
	template <wire_type T>
	[[nodiscard]] bool read_exact(std::span<const std::byte> body, T& out)
	{
		span_reader reader(body);
		return reader.read(out) && reader.at_end();
	}

	// A head plus a counted tail: exactly sizeof(Head) + count * sizeof(Record) bytes, where count_of(head)
	// gives the count and may not exceed max_count. The size is checked before anything is allocated.
	template <wire_type Head, wire_type Record, class CountOf>
	[[nodiscard]] bool read_counted(std::span<const std::byte> body, Head& head, std::vector<Record>& records, CountOf count_of,
		std::size_t max_count)
	{
		records.clear();
		span_reader reader(body);
		if (!reader.read(head))
			return false;
		const std::size_t count = static_cast<std::size_t>(count_of(head));
		if (count > max_count)
			return false;
		const std::size_t tail = reader.remaining();
		if (tail % sizeof(Record) != 0 || tail / sizeof(Record) != count)
			return false;
		return reader.read_array(count, records);
	}
}
