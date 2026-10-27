#pragma once

#include "common_types.h"

#include <cstdint>
#include <type_traits>

// Receipt checks for wire fields (plan sect. 4.3). A received struct is copied whole, so an enum or
// flag byte can hold any value the peer chose; these say whether it is one the protocol defines.
namespace hb::net
{
	template <class E>
		requires std::is_enum_v<E>
	constexpr std::underlying_type_t<E> underlying(E value)
	{
		return static_cast<std::underlying_type_t<E>>(value);
	}

	// True when value lies in [0, last]; every wire enum is contiguous from 0.
	template <class E>
		requires std::is_enum_v<E>
	constexpr bool enum_at_most(E value, E last)
	{
		static_assert(std::is_unsigned_v<std::underlying_type_t<E>>, "wire enums are unsigned");
		return underlying(value) <= underlying(last);
	}

	// A flag byte is 0 or 1; anything else is refused rather than read as true.
	constexpr bool is_flag(std::uint8_t value)
	{
		return value <= 1;
	}

	constexpr bool is_well_formed(const character_appearance& appearance)
	{
		return is_flag(appearance.hides_body_armour);
	}

	inline bool is_well_formed(const character_summary& summary)
	{
		return summary.name.is_zero_padded() && summary.map_name.is_zero_padded() && is_flag(summary.is_valid)
			&& is_well_formed(summary.appearance);
	}
}
