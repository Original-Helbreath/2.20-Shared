#pragma once

#include "common_types.h"
#include "field_checks.h"
#include "message_id.h"
#include "wire.h"

#include <cstddef>
#include <cstdint>

// Client <-> World handoff packets (plan sect. 4.6). After enter_game_response the client connects to the
// World address it was given and sends world_enter_request as its first frame; the World accepts it only for
// an unexpired, unused session_grant naming the same ticket and character, compares the ticket in constant
// time and consumes it once. Same receipt rule as login_messages.h: a body is accepted only if
// is_well_formed() holds. Game traffic after the handoff belongs to the World server plan.
namespace hb::net::world
{
	enum class world_enter_result : std::uint8_t
	{
		ok,
		bad_ticket,     // no grant for this ticket and character, or the ticket was already used
		expired,        // the grant's lifetime ran out before the client arrived
		world_full,
		shutting_down,
	};

	constexpr bool is_known(world_enter_result value) { return enum_at_most(value, world_enter_result::shutting_down); }

	#pragma pack(push, 1)
	struct world_enter_request  // client -> World, first frame
	{
		static constexpr message_id id = message_id::world_enter_request;
		session_ticket ticket;
		name_text character;
	};

	// Sent once; on anything but ok the World then closes the connection.
	struct world_enter_response
	{
		static constexpr message_id id = message_id::world_enter_response;
		world_enter_result result;
	};
	#pragma pack(pop)

	static_assert(sizeof(world_enter_request) == 48);
	static_assert(offsetof(world_enter_request, ticket) == 0);
	static_assert(offsetof(world_enter_request, character) == 32);

	static_assert(sizeof(world_enter_response) == 1);
	static_assert(offsetof(world_enter_response, result) == 0);

	static_assert(wire_type<world_enter_request> && wire_type<world_enter_response>);

	inline bool is_well_formed(const world_enter_request& m) { return m.character.is_zero_padded(); }

	constexpr bool is_well_formed(const world_enter_response& m) { return is_known(m.result); }
}
