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

#ifndef MPD_OGG_SYNC_STATE_HXX
#define MPD_OGG_SYNC_STATE_HXX

#include <ogg/ogg.h>

#include <stddef.h>

class Reader;

/**
 * Wrapper for an ogg_sync_state.
 */
class OggSyncState {
	ogg_sync_state oy;

	Reader &reader;

public:
	explicit OggSyncState(Reader &_reader)
		:reader(_reader) {
		ogg_sync_init(&oy);
	}

	~OggSyncState() {
		ogg_sync_clear(&oy);
	}

	OggSyncState(const OggSyncState &) = delete;
	OggSyncState &operator=(const OggSyncState &) = delete;

	void Reset() noexcept {
		ogg_sync_reset(&oy);
	}

	bool Feed(size_t size);

	bool ExpectPage(ogg_page &page);

	bool ExpectPageIn(ogg_stream_state &os);

	bool ExpectPageSeek(ogg_page &page);

	bool ExpectPageSeekIn(ogg_stream_state &os);
};

#endif
