#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <string_view>
#include <type_traits>

namespace hb::net
{
	// Fixed-width text on the wire, zero-padded. All N characters may be used (no terminator then).
	template <std::size_t N>
	struct fixed_string
	{
		static_assert(N > 0);

		std::array<char, N> chars{};

		// The text up to the first NUL, or all N characters.
		std::string_view view() const
		{
			const std::string_view all(chars.data(), N);
			return all.substr(0, all.find('\0'));
		}

		// False, leaving the field unchanged, if the text is longer than N or holds a NUL.
		bool assign(std::string_view text)
		{
			if (text.size() > N || text.find('\0') != std::string_view::npos)
				return false;
			chars.fill('\0');
			std::ranges::copy(text, chars.begin());
			return true;
		}

		// True when every byte after the first NUL is also NUL; received fields are checked with this
		// so no hidden bytes ride behind the visible text.
		bool is_zero_padded() const
		{
			const std::string_view all(chars.data(), N);
			const std::size_t end = all.find('\0');
			return end == std::string_view::npos || all.find_first_not_of('\0', end) == std::string_view::npos;
		}

		bool operator==(const fixed_string&) const = default;
	};

	static_assert(sizeof(fixed_string<16>) == 16);
	static_assert(std::is_trivially_copyable_v<fixed_string<16>> && std::is_standard_layout_v<fixed_string<16>>);
}
