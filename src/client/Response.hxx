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

#ifndef MPD_RESPONSE_HXX
#define MPD_RESPONSE_HXX

#include "protocol/Ack.hxx"
#include "util/Compiler.h"

#include <stddef.h>
#include <stdarg.h>

class Client;
class TagMask;

class Response {
	Client &client;

	/**
	 * This command's index in the command list.  Used to generate
	 * error messages.
	 */
	const unsigned list_index;

	/**
	 * This command's name.  Used to generate error messages.
	 */
	const char *command;

public:
	Response(Client &_client, unsigned _list_index)
		:client(_client), list_index(_list_index), command("") {}

	Response(const Response &) = delete;
	Response &operator=(const Response &) = delete;

	/**
	 * Returns a const reference to the associated #Client object.
	 * This should only be used to access a client's settings, to
	 * determine how to format the response.  For this reason, the
	 * returned reference is "const".
	 */
	const Client &GetClient() const {
		return client;
	}

	/**
	 * Accessor for Client::tag_mask.  Can be used if caller wants
	 * to avoid including Client.hxx.
	 */
	gcc_pure
	TagMask GetTagMask() const noexcept;

	void SetCommand(const char *_command) {
		command = _command;
	}

	bool Write(const void *data, size_t length);
	bool Write(const char *data);
	bool FormatV(const char *fmt, va_list args);
	bool Format(const char *fmt, ...);

	void Error(enum ack code, const char *msg);
	void FormatError(enum ack code, const char *fmt, ...);
};

#endif
