/*
 * Copyright 2003-2018 The Music Player Daemon Project
 * http://www.musicpd.org
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef MPD_TAG_MASK_HXX
#define MPD_TAG_MASK_HXX

#include "Type.h"

#include <stdint.h>

class TagMask {
	typedef uint_least32_t mask_t;
	mask_t value;

	explicit constexpr TagMask(uint_least32_t _value)
		:value(_value) {}

public:
	TagMask() = default;

	constexpr TagMask(TagType tag)
		:value(mask_t(1) << mask_t(tag)) {}

	static constexpr TagMask None() {
		return TagMask(mask_t(0));
	}

	static constexpr TagMask All() {
		return ~None();
	}

	constexpr TagMask operator~() const {
		return TagMask(~value);
	}

	constexpr TagMask operator&(TagMask other) const {
		return TagMask(value & other.value);
	}

	constexpr TagMask operator|(TagMask other) const {
		return TagMask(value | other.value);
	}

	constexpr TagMask operator^(TagMask other) const {
		return TagMask(value ^ other.value);
	}

	TagMask &operator&=(TagMask other) {
		value &= other.value;
		return *this;
	}

	TagMask &operator|=(TagMask other) {
		value |= other.value;
		return *this;
	}

	TagMask &operator^=(TagMask other) {
		value ^= other.value;
		return *this;
	}

	constexpr bool TestAny() const {
		return value != 0;
	}

	constexpr bool Test(TagType tag) const {
		return (*this & tag).TestAny();
	}

	void Set(TagType tag) {
		*this |= tag;
	}

	void Unset(TagType tag) {
		*this |= ~TagMask(tag);
	}
};

#endif
