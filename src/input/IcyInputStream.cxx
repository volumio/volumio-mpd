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

#include "IcyInputStream.hxx"
#include "IcyMetaDataParser.hxx"
#include "tag/Tag.hxx"

IcyInputStream::IcyInputStream(InputStreamPtr _input,
			       std::shared_ptr<IcyMetaDataParser> _parser) noexcept
	:ProxyInputStream(std::move(_input)), parser(std::move(_parser))
{
}

IcyInputStream::~IcyInputStream() noexcept = default;

inline bool
IcyInputStream::IsEnabled() const noexcept
{
	assert(parser);

	return parser->IsDefined();
}

void
IcyInputStream::Update() noexcept
{
	ProxyInputStream::Update();

	if (IsEnabled())
		offset = override_offset;
}

std::unique_ptr<Tag>
IcyInputStream::ReadTag()
{
	auto new_input_tag = ProxyInputStream::ReadTag();
	if (!IsEnabled())
		return new_input_tag;

	const bool had_new_input_tag = !!new_input_tag;
	if (new_input_tag != nullptr)
		input_tag = std::move(new_input_tag);

	auto new_icy_tag = parser->ReadTag();
	const bool had_new_icy_tag = !!new_icy_tag;
	if (new_icy_tag != nullptr)
		icy_tag = std::move(new_icy_tag);

	if (!had_new_input_tag && !had_new_icy_tag)
		/* no change */
		return nullptr;

	if (input_tag == nullptr && icy_tag == nullptr)
		/* no tag */
		return nullptr;

	if (input_tag == nullptr)
		return std::make_unique<Tag>(*icy_tag);

	if (icy_tag == nullptr)
		return std::make_unique<Tag>(*input_tag);

	return Tag::Merge(*input_tag, *icy_tag);
}

size_t
IcyInputStream::Read(void *ptr, size_t read_size)
{
	if (!IsEnabled())
		return ProxyInputStream::Read(ptr, read_size);

	while (true) {
		size_t nbytes = ProxyInputStream::Read(ptr, read_size);
		if (nbytes == 0)
			return 0;

		size_t result = parser->ParseInPlace(ptr, nbytes);
		if (result > 0) {
			override_offset += result;
			offset = override_offset;
			return result;
		}
	}
}
