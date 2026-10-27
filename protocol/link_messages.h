#pragma once

#include "common_types.h"
#include "field_checks.h"
#include "login_messages.h"
#include "message_id.h"
#include "wire.h"

#include <cstddef>
#include <cstdint>

// Login <-> World link packets (plan sect. 4.5). One connection per World server, opened by the World.
// Requests carry a request_id chosen by the sender that the reply echoes. Same receipt rule as
// login_messages.h: a body is accepted only if is_well_formed() holds.
namespace hb::net::link
{
	enum class register_result : std::uint8_t
	{
		ok,
		bad_key,
		world_name_taken,
		unknown_world,
		version_mismatch,
	};

	enum class grant_result : std::uint8_t
	{
		ok,
		world_full,
		no_such_character,
		shutting_down,
	};

	enum class session_end_reason : std::uint8_t
	{
		logout,
		disconnected,
		kicked,
		world_shutdown,
	};

	enum class kick_reason : std::uint8_t
	{
		forced_login_elsewhere,
		account_blocked,
		admin,
	};

	constexpr bool is_known(register_result value) { return enum_at_most(value, register_result::version_mismatch); }
	constexpr bool is_known(grant_result value) { return enum_at_most(value, grant_result::shutting_down); }
	constexpr bool is_known(session_end_reason value) { return enum_at_most(value, session_end_reason::world_shutdown); }
	constexpr bool is_known(kick_reason value) { return enum_at_most(value, kick_reason::admin); }

	#pragma pack(push, 1)
	struct world_register  // World -> Login, first frame
	{
		static constexpr message_id id = message_id::link_world_register;
		world_text world;
		host_text client_host;  // where clients connect to this World
		std::uint16_t client_port;
		link_key key;
		std::uint16_t max_players;
	};

	struct world_register_response
	{
		static constexpr message_id id = message_id::link_world_register_response;
		register_result result;
	};

	struct world_status  // on change and every 10 s
	{
		static constexpr message_id id = message_id::link_world_status;
		std::uint8_t is_accepting_players;  // flag
		std::uint8_t is_shutting_down;      // flag
		std::uint16_t player_count;
		std::uint16_t max_players;
	};

	struct character_list_request
	{
		static constexpr message_id id = message_id::link_character_list_request;
		std::uint32_t request_id;
		std::uint64_t account_id;
	};

	// Head of the frame; count x character_summary follow in the same body.
	struct character_list_response
	{
		static constexpr message_id id = message_id::link_character_list_response;
		std::uint32_t request_id;
		std::uint8_t max_slots;
		std::uint8_t count;
	};

	struct create_character_request
	{
		static constexpr message_id id = message_id::link_create_character_request;
		std::uint32_t request_id;
		std::uint64_t account_id;
		name_text name;
		std::uint8_t sex;
		std::uint8_t skin;
		std::uint8_t hair_style;
		std::uint8_t hair_colour;
		std::uint8_t underwear_colour;
		character_stats stats;
	};

	struct delete_character_request
	{
		static constexpr message_id id = message_id::link_delete_character_request;
		std::uint32_t request_id;
		std::uint64_t account_id;
		name_text name;
	};

	struct character_change_response  // answers create and delete
	{
		static constexpr message_id id = message_id::link_character_change_response;
		std::uint32_t request_id;
		login::character_result result;
	};

	struct session_grant
	{
		static constexpr message_id id = message_id::link_session_grant;
		std::uint32_t request_id;
		std::uint64_t account_id;
		name_text character;
		session_ticket ticket;
		std::uint16_t expires_in_seconds;
	};

	struct session_grant_response
	{
		static constexpr message_id id = message_id::link_session_grant_response;
		std::uint32_t request_id;
		grant_result result;
	};

	struct session_started  // also sent for every online player after world_register
	{
		static constexpr message_id id = message_id::link_session_started;
		std::uint64_t account_id;
		name_text character;
	};

	struct session_ended
	{
		static constexpr message_id id = message_id::link_session_ended;
		std::uint64_t account_id;
		session_end_reason reason;
	};

	struct kick_account
	{
		static constexpr message_id id = message_id::link_kick_account;
		std::uint64_t account_id;
		kick_reason reason;
	};

	// Every 5 s each way. One body, two ids (link_heartbeat_from_world / _from_login), so it has no
	// T::id; it is sent and read with an explicit id (message_codec.h).
	struct link_heartbeat
	{
		std::uint32_t sequence;
	};
	#pragma pack(pop)

	static_assert(sizeof(world_register) == 114);
	static_assert(offsetof(world_register, world) == 0);
	static_assert(offsetof(world_register, client_host) == 32);
	static_assert(offsetof(world_register, client_port) == 78);
	static_assert(offsetof(world_register, key) == 80);
	static_assert(offsetof(world_register, max_players) == 112);

	static_assert(sizeof(world_register_response) == 1);
	static_assert(offsetof(world_register_response, result) == 0);

	static_assert(sizeof(world_status) == 6);
	static_assert(offsetof(world_status, is_accepting_players) == 0);
	static_assert(offsetof(world_status, is_shutting_down) == 1);
	static_assert(offsetof(world_status, player_count) == 2);
	static_assert(offsetof(world_status, max_players) == 4);

	static_assert(sizeof(character_list_request) == 12);
	static_assert(offsetof(character_list_request, request_id) == 0);
	static_assert(offsetof(character_list_request, account_id) == 4);

	static_assert(sizeof(character_list_response) == 6);
	static_assert(offsetof(character_list_response, request_id) == 0);
	static_assert(offsetof(character_list_response, max_slots) == 4);
	static_assert(offsetof(character_list_response, count) == 5);

	static_assert(sizeof(create_character_request) == 39);
	static_assert(offsetof(create_character_request, request_id) == 0);
	static_assert(offsetof(create_character_request, account_id) == 4);
	static_assert(offsetof(create_character_request, name) == 12);
	static_assert(offsetof(create_character_request, sex) == 28);
	static_assert(offsetof(create_character_request, skin) == 29);
	static_assert(offsetof(create_character_request, hair_style) == 30);
	static_assert(offsetof(create_character_request, hair_colour) == 31);
	static_assert(offsetof(create_character_request, underwear_colour) == 32);
	static_assert(offsetof(create_character_request, stats) == 33);

	static_assert(sizeof(delete_character_request) == 28);
	static_assert(offsetof(delete_character_request, request_id) == 0);
	static_assert(offsetof(delete_character_request, account_id) == 4);
	static_assert(offsetof(delete_character_request, name) == 12);

	static_assert(sizeof(character_change_response) == 5);
	static_assert(offsetof(character_change_response, request_id) == 0);
	static_assert(offsetof(character_change_response, result) == 4);

	static_assert(sizeof(session_grant) == 62);
	static_assert(offsetof(session_grant, request_id) == 0);
	static_assert(offsetof(session_grant, account_id) == 4);
	static_assert(offsetof(session_grant, character) == 12);
	static_assert(offsetof(session_grant, ticket) == 28);
	static_assert(offsetof(session_grant, expires_in_seconds) == 60);

	static_assert(sizeof(session_grant_response) == 5);
	static_assert(offsetof(session_grant_response, request_id) == 0);
	static_assert(offsetof(session_grant_response, result) == 4);

	static_assert(sizeof(session_started) == 24);
	static_assert(offsetof(session_started, account_id) == 0);
	static_assert(offsetof(session_started, character) == 8);

	static_assert(sizeof(session_ended) == 9);
	static_assert(offsetof(session_ended, account_id) == 0);
	static_assert(offsetof(session_ended, reason) == 8);

	static_assert(sizeof(kick_account) == 9);
	static_assert(offsetof(kick_account, account_id) == 0);
	static_assert(offsetof(kick_account, reason) == 8);

	static_assert(sizeof(link_heartbeat) == 4);
	static_assert(offsetof(link_heartbeat, sequence) == 0);

	static_assert(wire_type<world_register> && wire_type<world_register_response> && wire_type<world_status>);
	static_assert(wire_type<character_list_request> && wire_type<character_list_response>);
	static_assert(wire_type<create_character_request> && wire_type<delete_character_request>);
	static_assert(wire_type<character_change_response> && wire_type<session_grant> && wire_type<session_grant_response>);
	static_assert(wire_type<session_started> && wire_type<session_ended> && wire_type<kick_account>);
	static_assert(wire_type<link_heartbeat>);

	inline bool is_well_formed(const world_register& m)
	{
		return m.world.is_zero_padded() && m.client_host.is_zero_padded();
	}

	constexpr bool is_well_formed(const world_register_response& m) { return is_known(m.result); }

	constexpr bool is_well_formed(const world_status& m)
	{
		return is_flag(m.is_accepting_players) && is_flag(m.is_shutting_down);
	}

	constexpr bool is_well_formed(const character_list_request&) { return true; }

	// The head alone; the counted tail is checked by decode_character_list (message_codec.h).
	constexpr bool is_well_formed(const character_list_response& m) { return m.count <= m.max_slots; }

	inline bool is_well_formed(const create_character_request& m) { return m.name.is_zero_padded(); }

	inline bool is_well_formed(const delete_character_request& m) { return m.name.is_zero_padded(); }

	constexpr bool is_well_formed(const character_change_response& m) { return login::is_known(m.result); }

	inline bool is_well_formed(const session_grant& m) { return m.character.is_zero_padded(); }

	constexpr bool is_well_formed(const session_grant_response& m) { return is_known(m.result); }

	inline bool is_well_formed(const session_started& m) { return m.character.is_zero_padded(); }

	constexpr bool is_well_formed(const session_ended& m) { return is_known(m.reason); }

	constexpr bool is_well_formed(const kick_account& m) { return is_known(m.reason); }

	constexpr bool is_well_formed(const link_heartbeat&) { return true; }
}
