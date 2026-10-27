#pragma once

#include "frame_header.h"
#include "wire.h"

#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

namespace hb::net
{
	// Why a frame was refused. Every value but none means the peer is closed (plan sect. 4.1).
	enum class frame_error : std::uint8_t
	{
		none,
		oversize,          // body_size above the configured max_body_bytes
		bad_version,       // protocol_version is not hb::net::protocol_version
		reserved_nonzero,  // reserved byte is not 0
		short_frame,       // fewer bytes than the header and its body_size
		long_frame,        // bytes left over after the body
		overrun,           // the peer sent more than the decoder will buffer (see frame_decoder)
	};

	std::string_view to_string(frame_error error);

	struct frame
	{
		frame_header header{};
		std::vector<std::byte> body;
	};

	// Every max_body_bytes taken below is first capped at max_body_ceiling (frame_header.h).

	// Checks the header alone, before any body bytes are waited for.
	[[nodiscard]] frame_error check_header(const frame_header& header, std::uint32_t max_body_bytes);

	// Appends header + body to out. False, leaving out unchanged, if the body exceeds max_body_bytes.
	[[nodiscard]] bool encode_frame(std::uint16_t message_id, std::span<const std::byte> body, std::uint32_t max_body_bytes,
		std::vector<std::byte>& out);

	// A whole message struct as the body.
	template <wire_type T>
	[[nodiscard]] bool encode_message(std::uint16_t message_id, const T& message, std::uint32_t max_body_bytes,
		std::vector<std::byte>& out)
	{
		return encode_frame(message_id, std::as_bytes(std::span<const T, 1>(&message, 1)), max_body_bytes, out);
	}

	// Exactly one frame in bytes; anything short or left over is an error.
	[[nodiscard]] frame_error decode_frame(std::span<const std::byte> bytes, std::uint32_t max_body_bytes, frame& out);

	enum class decode_status : std::uint8_t
	{
		frame_ready,
		need_more,
		failed,
	};

	// Splits a byte stream into frames. Feed it each read with append(), then call next() until it stops
	// returning frame_ready; that keeps at most one partial frame (header + max_body_bytes) buffered.
	// A header is checked as soon as its 8 bytes arrive, so an oversize body is refused before it is read.
	// append() walks every header as it completes (linear: it resumes where the last append stopped), so any
	// number of complete frames in one read is accepted; only the incomplete tail is bounded, at header +
	// max_body_bytes (overrun otherwise). Memory is therefore the caller's read size plus one partial frame,
	// provided the caller drains next() until it stops returning frame_ready after each append().
	// After failed the decoder stays failed and holds no buffer: the connection is closed, not resynchronised.
	class frame_decoder
	{
	public:
		explicit frame_decoder(std::uint32_t max_body_bytes);

		// False once the decoder has failed (bad header, overrun); error() says why.
		[[nodiscard]] bool append(std::span<const std::byte> bytes);
		[[nodiscard]] decode_status next(frame& out);

		frame_error error() const
		{
			return m_error;
		}

		std::size_t buffered() const
		{
			return m_buffer.size() - m_consumed;
		}

		std::uint32_t max_body_bytes() const
		{
			return m_max_body_bytes;
		}

	private:
		void fail(frame_error error);
		void compact();

		std::vector<std::byte> m_buffer;
		std::size_t m_consumed = 0;  // bytes already handed out by next()
		std::size_t m_scanned = 0;   // start of the first frame append() has not seen complete
		std::uint32_t m_max_body_bytes;
		frame_error m_error = frame_error::none;
	};
}
