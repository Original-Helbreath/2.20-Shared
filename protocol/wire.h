#pragma once

#include <bit>
#include <type_traits>

// Shared by the Login server, the World server and the client (login-server plan sect. 9).
namespace hb::net
{
	// Wire structs are copied byte for byte, so the protocol is little-endian on every host it builds for.
	static_assert(std::endian::native == std::endian::little, "the Helbreath protocol is little-endian");

	// A type that may cross the wire as a whole-object copy: no pointers, no padding bytes, and every
	// byte pattern of it is one value (so no bool or float fields; flags are std::uint8_t).
	template <class T>
	concept wire_type = std::is_trivially_copyable_v<T>
		&& std::is_standard_layout_v<T>
		&& std::has_unique_object_representations_v<T>
		&& !std::is_pointer_v<T>;
}
