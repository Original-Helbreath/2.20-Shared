#pragma once

#include "common_types.h"
#include "field_checks.h"
#include "message_id.h"
#include "wire.h"

#include <cstddef>
#include <cstdint>

// Client <-> Login packets (plan sect. 4.4). Each struct is one frame body, read and written whole;
// T::id is the frame's message id. A received body is accepted only if is_well_formed() holds:
// every enum in range, every flag 0 or 1, every text zero-padded. Game rules (name syntax, sex,
// skin, stat totals) are the Login server's to check, not the wire layer's.
namespace hb::net::login
{
	enum class login_result : std::uint8_t
	{
		ok,
		password_mismatch,
		no_such_account,
		no_such_world,
		account_blocked,
		account_locked,
		version_mismatch,
		world_unavailable,
		service_unavailable,
	};

	enum class account_result : std::uint8_t
	{
		ok,
		failed,
		name_taken,
		invalid_name,
		disabled,
	};

	enum class character_result : std::uint8_t
	{
		ok,
		failed,
		name_taken,
		invalid_name,
		invalid_stats,
		slots_full,
		no_such_character,
		level_too_high,
		world_unavailable,
	};

	enum class enter_kind : std::uint8_t
	{
		normal,
		force_login,
	};

	enum class enter_result : std::uint8_t
	{
		ok,
		already_playing,
		force_disconnect_sent,
		world_unavailable,
		world_full,
		world_shutting_down,
		no_such_character,
	};

	constexpr bool is_known(login_result value) { return enum_at_most(value, login_result::service_unavailable); }
	constexpr bool is_known(account_result value) { return enum_at_most(value, account_result::disabled); }
	constexpr bool is_known(character_result value) { return enum_at_most(value, character_result::world_unavailable); }
	constexpr bool is_known(enter_kind value) { return enum_at_most(value, enter_kind::force_login); }
	constexpr bool is_known(enter_result value) { return enum_at_most(value, enter_result::no_such_character); }

	#pragma pack(push, 1)
	struct login_request
	{
		static constexpr message_id id = message_id::login_request;
		name_text account;
		password_text password;
		world_text world;
	};

	// Followed, on ok, by a character_list frame.
	struct login_response
	{
		static constexpr message_id id = message_id::login_response;
		login_result result;
		std::uint8_t has_account_expiry;  // flag
		calendar_date account_expiry;
		calendar_date blocked_until;      // meaningful when result == account_blocked
	};

	// Head of the frame; count x character_summary follow in the same body.
	struct character_list
	{
		static constexpr message_id id = message_id::character_list;
		std::uint8_t max_slots;
		std::uint8_t count;
	};

	struct create_account_request  // only when enabled (plan Q7)
	{
		static constexpr message_id id = message_id::create_account_request;
		name_text account;
		password_text password;
		email_text email;
	};

	struct create_account_response
	{
		static constexpr message_id id = message_id::create_account_response;
		account_result result;
	};

	struct change_password_request  // allowed before login
	{
		static constexpr message_id id = message_id::change_password_request;
		name_text account;
		password_text old_password;
		password_text new_password;
	};

	struct change_password_response
	{
		static constexpr message_id id = message_id::change_password_response;
		account_result result;
	};

	struct create_character_request
	{
		static constexpr message_id id = message_id::create_character_request;
		name_text name;
		std::uint8_t sex;
		std::uint8_t skin;
		std::uint8_t hair_style;
		std::uint8_t hair_colour;
		std::uint8_t underwear_colour;
		character_stats stats;
	};

	// Followed, on ok, by a character_list frame.
	struct create_character_response
	{
		static constexpr message_id id = message_id::create_character_response;
		character_result result;
	};

	struct delete_character_request
	{
		static constexpr message_id id = message_id::delete_character_request;
		name_text name;
	};

	// Followed, on ok, by a character_list frame.
	struct delete_character_response
	{
		static constexpr message_id id = message_id::delete_character_response;
		character_result result;
	};

	struct enter_game_request
	{
		static constexpr message_id id = message_id::enter_game_request;
		name_text character;
		enter_kind kind;
	};

	struct enter_game_response
	{
		static constexpr message_id id = message_id::enter_game_response;
		enter_result result;
		host_text world_host;
		std::uint16_t world_port;
		session_ticket ticket;
	};
	#pragma pack(pop)

	static_assert(sizeof(login_request) == 80);
	static_assert(offsetof(login_request, account) == 0);
	static_assert(offsetof(login_request, password) == 16);
	static_assert(offsetof(login_request, world) == 48);

	static_assert(sizeof(login_response) == 10);
	static_assert(offsetof(login_response, result) == 0);
	static_assert(offsetof(login_response, has_account_expiry) == 1);
	static_assert(offsetof(login_response, account_expiry) == 2);
	static_assert(offsetof(login_response, blocked_until) == 6);

	static_assert(sizeof(character_list) == 2);
	static_assert(offsetof(character_list, max_slots) == 0);
	static_assert(offsetof(character_list, count) == 1);

	static_assert(sizeof(create_account_request) == 112);
	static_assert(offsetof(create_account_request, account) == 0);
	static_assert(offsetof(create_account_request, password) == 16);
	static_assert(offsetof(create_account_request, email) == 48);

	static_assert(sizeof(create_account_response) == 1);
	static_assert(offsetof(create_account_response, result) == 0);

	static_assert(sizeof(change_password_request) == 80);
	static_assert(offsetof(change_password_request, account) == 0);
	static_assert(offsetof(change_password_request, old_password) == 16);
	static_assert(offsetof(change_password_request, new_password) == 48);

	static_assert(sizeof(change_password_response) == 1);
	static_assert(offsetof(change_password_response, result) == 0);

	static_assert(sizeof(create_character_request) == 27);
	static_assert(offsetof(create_character_request, name) == 0);
	static_assert(offsetof(create_character_request, sex) == 16);
	static_assert(offsetof(create_character_request, skin) == 17);
	static_assert(offsetof(create_character_request, hair_style) == 18);
	static_assert(offsetof(create_character_request, hair_colour) == 19);
	static_assert(offsetof(create_character_request, underwear_colour) == 20);
	static_assert(offsetof(create_character_request, stats) == 21);

	static_assert(sizeof(create_character_response) == 1);
	static_assert(offsetof(create_character_response, result) == 0);

	static_assert(sizeof(delete_character_request) == 16);
	static_assert(offsetof(delete_character_request, name) == 0);

	static_assert(sizeof(delete_character_response) == 1);
	static_assert(offsetof(delete_character_response, result) == 0);

	static_assert(sizeof(enter_game_request) == 17);
	static_assert(offsetof(enter_game_request, character) == 0);
	static_assert(offsetof(enter_game_request, kind) == 16);

	static_assert(sizeof(enter_game_response) == 81);
	static_assert(offsetof(enter_game_response, result) == 0);
	static_assert(offsetof(enter_game_response, world_host) == 1);
	static_assert(offsetof(enter_game_response, world_port) == 47);
	static_assert(offsetof(enter_game_response, ticket) == 49);

	static_assert(wire_type<login_request> && wire_type<login_response> && wire_type<character_list>);
	static_assert(wire_type<create_account_request> && wire_type<create_account_response>);
	static_assert(wire_type<change_password_request> && wire_type<change_password_response>);
	static_assert(wire_type<create_character_request> && wire_type<create_character_response>);
	static_assert(wire_type<delete_character_request> && wire_type<delete_character_response>);
	static_assert(wire_type<enter_game_request> && wire_type<enter_game_response>);

	inline bool is_well_formed(const login_request& m)
	{
		return m.account.is_zero_padded() && m.password.is_zero_padded() && m.world.is_zero_padded();
	}

	constexpr bool is_well_formed(const login_response& m)
	{
		return is_known(m.result) && is_flag(m.has_account_expiry);
	}

	// The head alone; the counted tail is checked by decode_character_list (message_codec.h).
	constexpr bool is_well_formed(const character_list& m)
	{
		return m.count <= m.max_slots;
	}

	inline bool is_well_formed(const create_account_request& m)
	{
		return m.account.is_zero_padded() && m.password.is_zero_padded() && m.email.is_zero_padded();
	}

	constexpr bool is_well_formed(const create_account_response& m) { return is_known(m.result); }

	inline bool is_well_formed(const change_password_request& m)
	{
		return m.account.is_zero_padded() && m.old_password.is_zero_padded() && m.new_password.is_zero_padded();
	}

	constexpr bool is_well_formed(const change_password_response& m) { return is_known(m.result); }

	inline bool is_well_formed(const create_character_request& m) { return m.name.is_zero_padded(); }

	constexpr bool is_well_formed(const create_character_response& m) { return is_known(m.result); }

	inline bool is_well_formed(const delete_character_request& m) { return m.name.is_zero_padded(); }

	constexpr bool is_well_formed(const delete_character_response& m) { return is_known(m.result); }

	inline bool is_well_formed(const enter_game_request& m) { return m.character.is_zero_padded() && is_known(m.kind); }

	inline bool is_well_formed(const enter_game_response& m) { return is_known(m.result) && m.world_host.is_zero_padded(); }
}
