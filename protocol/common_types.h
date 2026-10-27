#pragma once

#include "fixed_string.h"
#include "wire.h"

#include <array>
#include <cstddef>
#include <cstdint>

// Field types shared by the client <-> Login and Login <-> World packets (plan sect. 4.3).
namespace hb::net
{
	using name_text = fixed_string<16>;      // account, character and map names (rule today: 1..10 chars)
	using password_text = fixed_string<32>;
	using world_text = fixed_string<32>;
	using host_text = fixed_string<46>;      // IPv4 or IPv6 text form
	using guild_text = fixed_string<24>;
	using email_text = fixed_string<64>;
	using session_ticket = std::array<std::uint8_t, 32>;
	using link_key = std::array<std::uint8_t, 32>;

	#pragma pack(push, 1)
	struct calendar_date
	{
		std::uint16_t year;
		std::uint8_t month;
		std::uint8_t day;
	};

	struct minute_stamp
	{
		std::uint16_t year;
		std::uint8_t month;
		std::uint8_t day;
		std::uint8_t hour;
		std::uint8_t minute;
	};

	struct character_stats
	{
		std::uint8_t strength;
		std::uint8_t vitality;
		std::uint8_t dexterity;
		std::uint8_t intelligence;
		std::uint8_t magic;
		std::uint8_t charisma;
	};

	// 2.20 packed this into appr1..appr4 (u16 each) and appr-color (i32); one byte per field here.
	struct character_appearance
	{
		std::uint8_t hair_style;         // 2.20 appr1 & 0x0F00
		std::uint8_t hair_colour;        // appr1 & 0x00F0
		std::uint8_t underwear_colour;   // appr1 & 0x000F
		std::uint8_t weapon;             // appr2 & 0x0FF0
		std::uint8_t shield;             // appr2 & 0x000F
		std::uint8_t body_armour;        // appr3 & 0xF000
		std::uint8_t pants;              // appr3 & 0x0F00
		std::uint8_t helm;               // appr3 & 0x00F0
		std::uint8_t arm_armour;         // appr3 & 0x000F
		std::uint8_t boots;              // appr4 & 0xF000
		std::uint8_t mantle;             // appr4 & 0x0F00
		std::uint8_t hides_body_armour;  // flag, appr4 & 0x80 (meaning inferred)
		std::uint8_t weapon_colour;      // appr-color bits 28-31
		std::uint8_t shield_colour;      // 24-27
		std::uint8_t armour_colour;      // 20-23
		std::uint8_t mantle_colour;      // 16-19
		std::uint8_t arm_colour;         // 12-15
		std::uint8_t pants_colour;       // 8-11
		std::uint8_t boots_colour;       // 4-7
		std::uint8_t helm_colour;        // 0-3
	};

	struct character_summary          // one select-screen slot; 2.20 slot record was 65 bytes
	{
		name_text name;
		std::uint8_t is_valid;            // flag; 2.20 "damaged file" byte
		std::uint8_t sex;                 // 1 male, 2 female
		std::uint8_t skin;                // 1..3
		std::uint16_t level;
		std::uint32_t experience;
		character_stats stats;
		character_appearance appearance;
		name_text map_name;
		minute_stamp last_saved;
	};
	#pragma pack(pop)

	static_assert(sizeof(name_text) == 16 && sizeof(password_text) == 32 && sizeof(world_text) == 32);
	static_assert(sizeof(host_text) == 46 && sizeof(guild_text) == 24 && sizeof(email_text) == 64);
	static_assert(sizeof(session_ticket) == 32 && sizeof(link_key) == 32);

	static_assert(sizeof(calendar_date) == 4);
	static_assert(offsetof(calendar_date, year) == 0);
	static_assert(offsetof(calendar_date, month) == 2);
	static_assert(offsetof(calendar_date, day) == 3);

	static_assert(sizeof(minute_stamp) == 6);
	static_assert(offsetof(minute_stamp, year) == 0);
	static_assert(offsetof(minute_stamp, month) == 2);
	static_assert(offsetof(minute_stamp, day) == 3);
	static_assert(offsetof(minute_stamp, hour) == 4);
	static_assert(offsetof(minute_stamp, minute) == 5);

	static_assert(sizeof(character_stats) == 6);
	static_assert(offsetof(character_stats, strength) == 0);
	static_assert(offsetof(character_stats, vitality) == 1);
	static_assert(offsetof(character_stats, dexterity) == 2);
	static_assert(offsetof(character_stats, intelligence) == 3);
	static_assert(offsetof(character_stats, magic) == 4);
	static_assert(offsetof(character_stats, charisma) == 5);

	static_assert(sizeof(character_appearance) == 20);
	static_assert(offsetof(character_appearance, hair_style) == 0);
	static_assert(offsetof(character_appearance, hair_colour) == 1);
	static_assert(offsetof(character_appearance, underwear_colour) == 2);
	static_assert(offsetof(character_appearance, weapon) == 3);
	static_assert(offsetof(character_appearance, shield) == 4);
	static_assert(offsetof(character_appearance, body_armour) == 5);
	static_assert(offsetof(character_appearance, pants) == 6);
	static_assert(offsetof(character_appearance, helm) == 7);
	static_assert(offsetof(character_appearance, arm_armour) == 8);
	static_assert(offsetof(character_appearance, boots) == 9);
	static_assert(offsetof(character_appearance, mantle) == 10);
	static_assert(offsetof(character_appearance, hides_body_armour) == 11);
	static_assert(offsetof(character_appearance, weapon_colour) == 12);
	static_assert(offsetof(character_appearance, shield_colour) == 13);
	static_assert(offsetof(character_appearance, armour_colour) == 14);
	static_assert(offsetof(character_appearance, mantle_colour) == 15);
	static_assert(offsetof(character_appearance, arm_colour) == 16);
	static_assert(offsetof(character_appearance, pants_colour) == 17);
	static_assert(offsetof(character_appearance, boots_colour) == 18);
	static_assert(offsetof(character_appearance, helm_colour) == 19);

	static_assert(sizeof(character_summary) == 73);
	static_assert(offsetof(character_summary, name) == 0);
	static_assert(offsetof(character_summary, is_valid) == 16);
	static_assert(offsetof(character_summary, sex) == 17);
	static_assert(offsetof(character_summary, skin) == 18);
	static_assert(offsetof(character_summary, level) == 19);
	static_assert(offsetof(character_summary, experience) == 21);
	static_assert(offsetof(character_summary, stats) == 25);
	static_assert(offsetof(character_summary, appearance) == 31);
	static_assert(offsetof(character_summary, map_name) == 51);
	static_assert(offsetof(character_summary, last_saved) == 67);

	static_assert(wire_type<calendar_date> && wire_type<minute_stamp> && wire_type<character_stats>);
	static_assert(wire_type<character_appearance> && wire_type<character_summary>);
	static_assert(wire_type<session_ticket> && wire_type<name_text> && wire_type<email_text>);
}
