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

#include "config.h"
#include "PlaylistRegistry.hxx"
#include "PlaylistPlugin.hxx"
#include "SongEnumerator.hxx"
#include "plugins/ExtM3uPlaylistPlugin.hxx"
#include "plugins/M3uPlaylistPlugin.hxx"
#include "plugins/XspfPlaylistPlugin.hxx"
#include "plugins/SoundCloudPlaylistPlugin.hxx"
#include "plugins/PlsPlaylistPlugin.hxx"
#include "plugins/AsxPlaylistPlugin.hxx"
#include "plugins/RssPlaylistPlugin.hxx"
#include "plugins/FlacPlaylistPlugin.hxx"
#include "plugins/CuePlaylistPlugin.hxx"
#include "plugins/EmbeddedCuePlaylistPlugin.hxx"
#include "input/InputStream.hxx"
#include "util/MimeType.hxx"
#include "util/UriUtil.hxx"
#include "util/StringUtil.hxx"
#include "util/Macros.hxx"
#include "config/Data.hxx"
#include "config/Block.hxx"

#include <assert.h>
#include <string.h>

const struct playlist_plugin *const playlist_plugins[] = {
	&extm3u_playlist_plugin,
	&m3u_playlist_plugin,
	&pls_playlist_plugin,
#ifdef ENABLE_EXPAT
	&xspf_playlist_plugin,
	&asx_playlist_plugin,
	&rss_playlist_plugin,
#endif
#ifdef ENABLE_SOUNDCLOUD
	&soundcloud_playlist_plugin,
#endif
#ifdef ENABLE_FLAC
	&flac_playlist_plugin,
#endif
#ifdef ENABLE_CUE
	&cue_playlist_plugin,
	&embcue_playlist_plugin,
#endif
	nullptr
};

static constexpr unsigned n_playlist_plugins =
	ARRAY_SIZE(playlist_plugins) - 1;

/** which plugins have been initialized successfully? */
static bool playlist_plugins_enabled[n_playlist_plugins];

#define playlist_plugins_for_each_enabled(plugin) \
	playlist_plugins_for_each(plugin) \
		if (playlist_plugins_enabled[playlist_plugin_iterator - playlist_plugins])

void
playlist_list_global_init(const ConfigData &config)
{
	const ConfigBlock empty;

	for (unsigned i = 0; playlist_plugins[i] != nullptr; ++i) {
		const struct playlist_plugin *plugin = playlist_plugins[i];
		const auto *param =
			config.FindBlock(ConfigBlockOption::PLAYLIST_PLUGIN,
					 "name", plugin->name);
		if (param == nullptr)
			param = &empty;
		else if (!param->GetBlockValue("enabled", true))
			/* the plugin is disabled in mpd.conf */
			continue;

		if (param != nullptr)
			param->SetUsed();

		playlist_plugins_enabled[i] =
			playlist_plugin_init(playlist_plugins[i], *param);
	}
}

void
playlist_list_global_finish() noexcept
{
	playlist_plugins_for_each_enabled(plugin)
		playlist_plugin_finish(plugin);
}

static std::unique_ptr<SongEnumerator>
playlist_list_open_uri_scheme(const char *uri, Mutex &mutex,
			      bool *tried)
{
	assert(uri != nullptr);

	const auto scheme = uri_get_scheme(uri);
	if (scheme.empty())
		return nullptr;

	for (unsigned i = 0; playlist_plugins[i] != nullptr; ++i) {
		const struct playlist_plugin *plugin = playlist_plugins[i];

		assert(!tried[i]);

		if (playlist_plugins_enabled[i] && plugin->open_uri != nullptr &&
		    plugin->schemes != nullptr &&
		    StringArrayContainsCase(plugin->schemes, scheme.c_str())) {
			auto playlist = plugin->open_uri(uri, mutex);
			if (playlist)
				return playlist;

			tried[i] = true;
		}
	}

	return nullptr;
}

static std::unique_ptr<SongEnumerator>
playlist_list_open_uri_suffix(const char *uri, Mutex &mutex,
			      const bool *tried)
{
	assert(uri != nullptr);

	UriSuffixBuffer suffix_buffer;
	const char *const suffix = uri_get_suffix(uri, suffix_buffer);
	if (suffix == nullptr)
		return nullptr;

	for (unsigned i = 0; playlist_plugins[i] != nullptr; ++i) {
		const struct playlist_plugin *plugin = playlist_plugins[i];

		if (playlist_plugins_enabled[i] && !tried[i] &&
		    plugin->open_uri != nullptr && plugin->suffixes != nullptr &&
		    StringArrayContainsCase(plugin->suffixes, suffix)) {
			auto playlist = plugin->open_uri(uri, mutex);
			if (playlist != nullptr)
				return playlist;
		}
	}

	return nullptr;
}

std::unique_ptr<SongEnumerator>
playlist_list_open_uri(const char *uri, Mutex &mutex)
{
	/** this array tracks which plugins have already been tried by
	    playlist_list_open_uri_scheme() */
	bool tried[n_playlist_plugins];

	assert(uri != nullptr);

	memset(tried, false, sizeof(tried));

	auto playlist = playlist_list_open_uri_scheme(uri, mutex, tried);
	if (playlist == nullptr)
		playlist = playlist_list_open_uri_suffix(uri, mutex,
							 tried);

	return playlist;
}

static std::unique_ptr<SongEnumerator>
playlist_list_open_stream_mime2(InputStreamPtr &&is, const char *mime)
{
	assert(mime != nullptr);

	playlist_plugins_for_each_enabled(plugin) {
		if (plugin->open_stream != nullptr &&
		    plugin->mime_types != nullptr &&
		    StringArrayContainsCase(plugin->mime_types, mime)) {
			/* rewind the stream, so each plugin gets a
			   fresh start */
			try {
				is->LockRewind();
			} catch (...) {
			}

			auto playlist = plugin->open_stream(std::move(is));
			if (playlist != nullptr)
				return playlist;
		}
	}

	return nullptr;
}

static std::unique_ptr<SongEnumerator>
playlist_list_open_stream_mime(InputStreamPtr &&is, const char *full_mime)
{
	assert(full_mime != nullptr);

	const char *semicolon = strchr(full_mime, ';');
	if (semicolon == nullptr)
		return playlist_list_open_stream_mime2(std::move(is),
						       full_mime);

	if (semicolon == full_mime)
		return nullptr;

	/* probe only the portion before the semicolon*/
	const std::string mime(full_mime, semicolon);
	return playlist_list_open_stream_mime2(std::move(is), mime.c_str());
}

std::unique_ptr<SongEnumerator>
playlist_list_open_stream_suffix(InputStreamPtr &&is, const char *suffix)
{
	assert(suffix != nullptr);

	playlist_plugins_for_each_enabled(plugin) {
		if (plugin->open_stream != nullptr &&
		    plugin->suffixes != nullptr &&
		    StringArrayContainsCase(plugin->suffixes, suffix)) {
			/* rewind the stream, so each plugin gets a
			   fresh start */
			try {
				is->LockRewind();
			} catch (...) {
			}

			auto playlist = plugin->open_stream(std::move(is));
			if (playlist != nullptr)
				return playlist;
		}
	}

	return nullptr;
}

std::unique_ptr<SongEnumerator>
playlist_list_open_stream(InputStreamPtr &&is, const char *uri)
{
	assert(is->IsReady());

	const char *const mime = is->GetMimeType();
	if (mime != nullptr) {
		auto playlist = playlist_list_open_stream_mime(std::move(is),
							       GetMimeTypeBase(mime).c_str());
		if (playlist != nullptr)
			return playlist;
	}

	UriSuffixBuffer suffix_buffer;
	const char *suffix = uri != nullptr
		? uri_get_suffix(uri, suffix_buffer)
		: nullptr;
	if (suffix != nullptr) {
		auto playlist = playlist_list_open_stream_suffix(std::move(is),
								 suffix);
		if (playlist != nullptr)
			return playlist;
	}

	return nullptr;
}

bool
playlist_suffix_supported(const char *suffix) noexcept
{
	assert(suffix != nullptr);

	playlist_plugins_for_each_enabled(plugin) {
		if (plugin->suffixes != nullptr &&
		    StringArrayContainsCase(plugin->suffixes, suffix))
			return true;
	}

	return false;
}
