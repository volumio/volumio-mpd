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

#include "Response.hxx"
#include "Client.hxx"
#include "util/FormatString.hxx"
#include "util/AllocatedString.hxx"

TagMask
Response::GetTagMask() const noexcept
{
	return GetClient().tag_mask;
}

bool
Response::Write(const void *data, size_t length)
{
	return client.Write(data, length);
}

bool
Response::Write(const char *data)
{
	return client.Write(data);
}

bool
Response::FormatV(const char *fmt, va_list args)
{
	return Write(FormatStringV(fmt, args).c_str());
}

bool
Response::Format(const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	bool success = FormatV(fmt, args);
	va_end(args);
	return success;
}

void
Response::Error(enum ack code, const char *msg)
{
	FormatError(code, "%s", msg);
}

void
Response::FormatError(enum ack code, const char *fmt, ...)
{
	Format("ACK [%i@%u] {%s} ",
	       (int)code, list_index, command);

	va_list args;
	va_start(args, fmt);
	FormatV(fmt, args);
	va_end(args);

	Write("\n");
}
