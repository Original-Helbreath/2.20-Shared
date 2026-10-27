#pragma once

#include "wire.h"

#include <cstddef>
#include <cstdint>

namespace hb::net
{
	// The release this build speaks; a peer sending another value is refused at its first frame.
	inline constexpr std::uint8_t protocol_version = 1;

	// Hard ceiling on any configured max_body_bytes (16 MiB). It keeps every size sum (header + body)
	// far inside a 32-bit size_t, for the x86 client.
	inline constexpr std::uint32_t max_body_ceiling = 16u * 1024u * 1024u;

	constexpr std::uint32_t capped_body_bytes(std::uint32_t max_body_bytes)
	{
		return max_body_bytes < max_body_ceiling ? max_body_bytes : max_body_ceiling;
	}

	// Precedes every message on every link (plan sect. 4.1). Id ranges: 0x01xx client -> Login,
	// 0x02xx Login -> client, 0x03xx World -> Login, 0x04xx Login -> World, 0x05xx client <-> World.
	#pragma pack(push, 1)
	struct frame_header
	{
		std::uint32_t body_size;        // bytes after this header
		std::uint16_t message_id;       // hb::net::message_id (LOGIN-004)
		std::uint8_t protocol_version;  // hb::net::protocol_version
		std::uint8_t reserved;          // 0; rejected if not 0
	};
	#pragma pack(pop)

	static_assert(sizeof(frame_header) == 8);
	static_assert(offsetof(frame_header, body_size) == 0);
	static_assert(offsetof(frame_header, message_id) == 4);
	static_assert(offsetof(frame_header, protocol_version) == 6);
	static_assert(offsetof(frame_header, reserved) == 7);
	static_assert(wire_type<frame_header>);
}
