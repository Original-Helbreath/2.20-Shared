#pragma once

#include <cstdint>

// Every message id of the Login protocol (plan sect. 4.1, 4.4, 4.5). The high byte is the direction:
// 0x01 client -> Login, 0x02 Login -> client, 0x03 World -> Login, 0x04 Login -> World, 0x05 client ->
// World (0x0501, 0x0502: the ticket handoff of plan sect. 4.6, world_messages.h) and World -> client.
// Game traffic after the handoff rides 0x0503 / 0x0504, a temporary envelope for the legacy game messages.
namespace hb::net
{
	enum class message_id : std::uint16_t
	{
		// client -> Login
		login_request = 0x0101,
		create_account_request = 0x0102,
		change_password_request = 0x0103,
		create_character_request = 0x0104,
		delete_character_request = 0x0105,
		enter_game_request = 0x0106,

		// Login -> client
		login_response = 0x0201,
		character_list = 0x0202,
		create_account_response = 0x0203,
		change_password_response = 0x0204,
		create_character_response = 0x0205,
		delete_character_response = 0x0206,
		enter_game_response = 0x0207,

		// World -> Login
		link_world_register = 0x0301,
		link_world_status = 0x0302,
		link_character_list_response = 0x0303,
		link_character_change_response = 0x0304,
		link_session_grant_response = 0x0305,
		link_session_started = 0x0306,
		link_session_ended = 0x0307,
		link_heartbeat_from_world = 0x0308,

		// Login -> World
		link_world_register_response = 0x0401,
		link_character_list_request = 0x0402,
		link_create_character_request = 0x0403,
		link_delete_character_request = 0x0404,
		link_session_grant = 0x0405,
		link_kick_account = 0x0406,
		link_heartbeat_from_login = 0x0408,

		// client -> World, and World -> client
		world_enter_request = 0x0501,
		world_enter_response = 0x0502,

		// Temporary legacy envelope (SHARED-003): after world_enter_response ok, each frame carries exactly one
		// legacy 2.20 game message as its body (DWORD msg id, WORD msg type, payload), no cipher, at most 65536
		// bytes. These two ids go away as each game message is converted to a typed struct.
		world_game_message = 0x0503,   // client -> World
		client_game_message = 0x0504,  // World -> client
	};

	enum class message_direction : std::uint8_t
	{
		unknown,
		client_to_login,
		login_to_client,
		world_to_login,
		login_to_world,
		client_to_world,
		world_to_client,
	};

	// The direction of a known id; unknown for any value this protocol does not define.
	constexpr message_direction direction_of(std::uint16_t raw)
	{
		switch (static_cast<message_id>(raw))
		{
		case message_id::login_request:
		case message_id::create_account_request:
		case message_id::change_password_request:
		case message_id::create_character_request:
		case message_id::delete_character_request:
		case message_id::enter_game_request:
			return message_direction::client_to_login;
		case message_id::login_response:
		case message_id::character_list:
		case message_id::create_account_response:
		case message_id::change_password_response:
		case message_id::create_character_response:
		case message_id::delete_character_response:
		case message_id::enter_game_response:
			return message_direction::login_to_client;
		case message_id::link_world_register:
		case message_id::link_world_status:
		case message_id::link_character_list_response:
		case message_id::link_character_change_response:
		case message_id::link_session_grant_response:
		case message_id::link_session_started:
		case message_id::link_session_ended:
		case message_id::link_heartbeat_from_world:
			return message_direction::world_to_login;
		case message_id::link_world_register_response:
		case message_id::link_character_list_request:
		case message_id::link_create_character_request:
		case message_id::link_delete_character_request:
		case message_id::link_session_grant:
		case message_id::link_kick_account:
		case message_id::link_heartbeat_from_login:
			return message_direction::login_to_world;
		case message_id::world_enter_request:
		case message_id::world_game_message:
			return message_direction::client_to_world;
		case message_id::world_enter_response:
		case message_id::client_game_message:
			return message_direction::world_to_client;
		}
		return message_direction::unknown;
	}

	// True when raw is a defined id; a receiver also checks direction_of() against the link it came on.
	constexpr bool is_known(std::uint16_t raw)
	{
		return direction_of(raw) != message_direction::unknown;
	}

	constexpr std::uint16_t to_wire(message_id id)
	{
		return static_cast<std::uint16_t>(id);
	}
}
