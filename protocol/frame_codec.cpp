#include "frame_codec.h"

#include "span_reader.h"
#include "span_writer.h"

#include <iterator>

namespace hb::net
{
	std::string_view to_string(frame_error error)
	{
		switch (error)
		{
		case frame_error::none:
			return "none";
		case frame_error::oversize:
			return "oversize";
		case frame_error::bad_version:
			return "bad_version";
		case frame_error::reserved_nonzero:
			return "reserved_nonzero";
		case frame_error::short_frame:
			return "short_frame";
		case frame_error::long_frame:
			return "long_frame";
		case frame_error::overrun:
			return "overrun";
		}
		return "unknown";
	}

	frame_error check_header(const frame_header& header, std::uint32_t max_body_bytes)
	{
		if (header.protocol_version != protocol_version)
			return frame_error::bad_version;
		if (header.reserved != 0)
			return frame_error::reserved_nonzero;
		if (header.body_size > capped_body_bytes(max_body_bytes))
			return frame_error::oversize;
		return frame_error::none;
	}

	bool encode_frame(std::uint16_t message_id, std::span<const std::byte> body, std::uint32_t max_body_bytes,
		std::vector<std::byte>& out)
	{
		if (body.size() > capped_body_bytes(max_body_bytes))
			return false;
		static_assert(sizeof(std::size_t) >= sizeof(std::uint32_t));
		const frame_header header{ static_cast<std::uint32_t>(body.size()), message_id, protocol_version, 0 };
		out.reserve(out.size() + sizeof(header) + body.size());
		span_writer writer;
		writer.write(header);
		const std::span<const std::byte> head = writer.bytes();
		out.insert(out.end(), head.begin(), head.end());
		out.insert(out.end(), body.begin(), body.end());
		return true;
	}

	frame_error decode_frame(std::span<const std::byte> bytes, std::uint32_t max_body_bytes, frame& out)
	{
		span_reader reader(bytes);
		frame_header header{};
		if (!reader.read(header))
			return frame_error::short_frame;
		if (const frame_error error = check_header(header, max_body_bytes); error != frame_error::none)
			return error;
		if (reader.remaining() < header.body_size)
			return frame_error::short_frame;
		if (reader.remaining() > header.body_size)
			return frame_error::long_frame;
		if (!reader.read_array(header.body_size, out.body))
			return frame_error::short_frame;
		out.header = header;
		return frame_error::none;
	}

	frame_decoder::frame_decoder(std::uint32_t max_body_bytes)
		: m_max_body_bytes(capped_body_bytes(max_body_bytes))
	{
	}

	bool frame_decoder::append(std::span<const std::byte> bytes)
	{
		if (m_error != frame_error::none)
			return false;
		compact();
		m_buffer.insert(m_buffer.end(), bytes.begin(), bytes.end());
		// Walk the headers from where the last append stopped, so each byte is scanned once. Every header
		// is checked as soon as it is complete; complete frames are accepted and only the tail is bounded.
		for (;;)
		{
			span_reader reader(std::span<const std::byte>(m_buffer).subspan(m_scanned));
			frame_header header{};
			if (!reader.read(header))
				break;
			if (const frame_error error = check_header(header, m_max_body_bytes); error != frame_error::none)
			{
				fail(error);
				return false;
			}
			if (reader.remaining() < header.body_size)
				break;
			m_scanned += sizeof(frame_header) + header.body_size;
		}
		// The incomplete tail. With every header checked above it cannot pass one maximum frame; the check
		// keeps that bound explicit should the header rules ever change.
		if (m_buffer.size() - m_scanned > sizeof(frame_header) + std::size_t{ m_max_body_bytes })
		{
			fail(frame_error::overrun);
			return false;
		}
		return true;
	}

	void frame_decoder::fail(frame_error error)
	{
		m_error = error;
		m_buffer.clear();
		m_buffer.shrink_to_fit();
		m_consumed = 0;
		m_scanned = 0;
	}

	decode_status frame_decoder::next(frame& out)
	{
		if (m_error != frame_error::none)
			return decode_status::failed;
		span_reader reader(std::span<const std::byte>(m_buffer).subspan(m_consumed));
		frame_header header{};
		if (!reader.read(header))
		{
			compact();
			return decode_status::need_more;
		}
		if (const frame_error error = check_header(header, m_max_body_bytes); error != frame_error::none)
		{
			fail(error);
			return decode_status::failed;
		}
		if (reader.remaining() < header.body_size)
		{
			compact();
			return decode_status::need_more;
		}
		if (!reader.read_array(header.body_size, out.body))
		{
			fail(frame_error::short_frame);
			return decode_status::failed;
		}
		out.header = header;
		m_consumed += sizeof(frame_header) + header.body_size;
		if (m_consumed == m_buffer.size())
		{
			m_buffer.clear();
			m_consumed = 0;
			m_scanned = 0;
		}
		return decode_status::frame_ready;
	}

	void frame_decoder::compact()
	{
		if (m_consumed == 0)
			return;
		// next() only consumes complete frames, which append() has already scanned: m_consumed <= m_scanned.
		m_buffer.erase(m_buffer.begin(), std::next(m_buffer.begin(), static_cast<std::ptrdiff_t>(m_consumed)));
		m_scanned -= m_consumed;
		m_consumed = 0;
	}
}
