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

#ifndef MPD_DECODER_PLUGIN_HXX
#define MPD_DECODER_PLUGIN_HXX

#include "util/Compiler.h"

#include <forward_list>

struct ConfigBlock;
class InputStream;
class TagHandler;
class Path;
class DecoderClient;
class DetachedSong;

struct DecoderPlugin {
	const char *name;

	/**
	 * Initialize the decoder plugin.  Optional method.
	 *
	 * @param param a configuration block for this plugin, or nullptr
	 * if none is configured
	 * @return true if the plugin was initialized successfully,
	 * false if the plugin is not available
	 */
	bool (*init)(const ConfigBlock &block);

	/**
	 * Deinitialize a decoder plugin which was initialized
	 * successfully.  Optional method.
	 */
	void (*finish)() noexcept;

	/**
	 * Decode a stream (data read from an #InputStream object).
	 *
	 * Either implement this method or file_decode().  If
	 * possible, it is recommended to implement this method,
	 * because it is more versatile.
	 */
	void (*stream_decode)(DecoderClient &client, InputStream &is);

	/**
	 * Decode a local file.
	 *
	 * Either implement this method or stream_decode().
	 */
	void (*file_decode)(DecoderClient &client, Path path_fs);

	/**
	 * Scan metadata of a file.
	 *
	 * @return false if the operation has failed
	 */
	bool (*scan_file)(Path path_fs, TagHandler &handler) noexcept;

	/**
	 * Scan metadata of a file.
	 *
	 * @return false if the operation has failed
	 */
	bool (*scan_stream)(InputStream &is, TagHandler &handler) noexcept;

	/**
	 * @brief Return a "virtual" filename for subtracks in
	 * container formats like flac
	 * @param path_fs full pathname for the file on fs
	 *
	 * @return an empty list if there are no multiple files
	 * a filename for every single track;
	 * do not include full pathname here, just the "virtual" file
	 */
	std::forward_list<DetachedSong> (*container_scan)(Path path_fs);

	/* last element in these arrays must always be a nullptr: */
	const char *const*suffixes;
	const char *const*mime_types;

	/**
	 * Initialize a decoder plugin.
	 *
	 * @param block a configuration block for this plugin
	 * @return true if the plugin was initialized successfully, false if
	 * the plugin is not available
	 */
	bool Init(const ConfigBlock &block) const {
		return init != nullptr
			? init(block)
			: true;
	}

	/**
	 * Deinitialize a decoder plugin which was initialized successfully.
	 */
	void Finish() const {
		if (finish != nullptr)
			finish();
	}

	/**
	 * Decode a stream.
	 */
	void StreamDecode(DecoderClient &client, InputStream &is) const {
		stream_decode(client, is);
	}

	/**
	 * Decode a file.
	 */
	template<typename P>
	void FileDecode(DecoderClient &client, P path_fs) const {
		file_decode(client, path_fs);
	}

	/**
	 * Read the tag of a file.
	 */
	template<typename P>
	bool ScanFile(P path_fs, TagHandler &handler) const noexcept {
		return scan_file != nullptr
			? scan_file(path_fs, handler)
			: false;
	}

	/**
	 * Read the tag of a stream.
	 */
	bool ScanStream(InputStream &is, TagHandler &handler) const noexcept {
		return scan_stream != nullptr
			? scan_stream(is, handler)
			: false;
	}

	/**
	 * return "virtual" tracks in a container
	 */
	template<typename P>
	char *ContainerScan(P path, const unsigned int tnum) const {
		return container_scan(path, tnum);
	}

	/**
	 * Does the plugin announce the specified file name suffix?
	 */
	gcc_pure gcc_nonnull_all
	bool SupportsSuffix(const char *suffix) const noexcept;

	/**
	 * Does the plugin announce the specified MIME type?
	 */
	gcc_pure gcc_nonnull_all
	bool SupportsMimeType(const char *mime_type) const noexcept;
};

#endif
