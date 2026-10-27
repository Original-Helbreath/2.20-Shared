#pragma once

#include "common_types.h"
#include "field_checks.h"
#include "frame_codec.h"
#include "link_messages.h"
#include "login_messages.h"
#include "message_id.h"
#include "span_reader.h"
#include "span_writer.h"
#include "wire.h"

#include <algorithm>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

// Typed send and receive over frame_codec: a message struct goes out under its own T::id, and comes in
// only if the id matches, the body is exactly sizeof(T) and is_well_formed() holds.
namespace hb::net
{
	template <class T>
	concept identified_message = wire_type<T> && requires {
		{ T::id } -> std::convertible_to<message_id>;
	};

	// A fixed-size body, accepted only when well formed (is_well_formed is found by ADL).
	template <wire_type T>
	[[nodiscard]] bool read_message(std::span<const std::byte> body, T& out)
	{
		return read_exact(body, out) && is_well_formed(out);
	}

	template <identified_message T>
	[[nodiscard]] bool encode(const T& message, std::uint32_t max_body_bytes, std::vector<std::byte>& out)
	{
		return encode_message(to_wire(T::id), message, max_body_bytes, out);
	}

	template <identified_message T>
	[[nodiscard]] bool decode(const frame& in, T& out)
	{
		return in.header.message_id == to_wire(T::id) && read_message(std::span<const std::byte>(in.body), out);
	}

	// The heartbeat has one body and two ids, so the id is named by the caller.
	[[nodiscard]] inline bool encode_heartbeat(message_id id, const link::link_heartbeat& message, std::uint32_t max_body_bytes,
		std::vector<std::byte>& out)
	{
		if (id != message_id::link_heartbeat_from_world && id != message_id::link_heartbeat_from_login)
			return false;
		return encode_message(to_wire(id), message, max_body_bytes, out);
	}

	[[nodiscard]] inline bool decode_heartbeat(const frame& in, message_id expected, link::link_heartbeat& out)
	{
		if (expected != message_id::link_heartbeat_from_world && expected != message_id::link_heartbeat_from_login)
			return false;
		return in.header.message_id == to_wire(expected) && read_message(std::span<const std::byte>(in.body), out);
	}

	// A list head (login::character_list or link::character_list_response) and its count x character_summary.
	template <class Head>
	concept character_list_head = identified_message<Head> && requires(const Head& h) {
		{ h.count } -> std::convertible_to<std::uint8_t>;
		{ h.max_slots } -> std::convertible_to<std::uint8_t>;
	};

	// False, leaving out unchanged, if summaries.size() differs from head.count, the head or any summary
	// is not well formed, or the body exceeds max_body_bytes. It refuses exactly what the decoder refuses.
	template <character_list_head Head>
	[[nodiscard]] bool encode_character_list(const Head& head, std::span<const character_summary> summaries,
		std::uint32_t max_body_bytes, std::vector<std::byte>& out)
	{
		const auto well_formed = [](const character_summary& s) { return is_well_formed(s); };
		if (summaries.size() != head.count || !is_well_formed(head) || !std::ranges::all_of(summaries, well_formed))
			return false;
		span_writer body;
		body.write(head);
		body.write_array(summaries);
		const std::vector<std::byte> bytes = body.take();
		return encode_frame(to_wire(Head::id), bytes, max_body_bytes, out);
	}

	// Accepts the frame only if its id is Head::id, the head is well formed (count <= max_slots), the body
	// is exactly the head plus count summaries, and every summary is well formed. On failure head is
	// reset to Head{} and summaries is empty, so no part of a refused frame is left behind.
	template <character_list_head Head>
	[[nodiscard]] bool decode_character_list(const frame& in, Head& head, std::vector<character_summary>& summaries)
	{
		const auto refuse = [&] {
			head = Head{};
			summaries.clear();
			return false;
		};
		summaries.clear();
		if (in.header.message_id != to_wire(Head::id))
			return refuse();
		const auto count_of = [](const Head& h) { return h.count; };
		if (!read_counted(std::span<const std::byte>(in.body), head, summaries, count_of, 255))
			return refuse();
		const auto well_formed = [](const character_summary& s) { return is_well_formed(s); };
		if (!is_well_formed(head) || !std::ranges::all_of(summaries, well_formed))
			return refuse();
		return true;
	}
}
