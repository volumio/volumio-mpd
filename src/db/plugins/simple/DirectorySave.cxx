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

#include "DirectorySave.hxx"
#include "Directory.hxx"
#include "Song.hxx"
#include "SongSave.hxx"
#include "song/DetachedSong.hxx"
#include "PlaylistDatabase.hxx"
#include "fs/io/TextFile.hxx"
#include "fs/io/BufferedOutputStream.hxx"
#include "time/ChronoUtil.hxx"
#include "util/StringCompare.hxx"
#include "util/NumberParser.hxx"
#include "util/RuntimeError.hxx"

#include <string.h>

#define DIRECTORY_DIR "directory: "
#define DIRECTORY_TYPE "type: "
#define DIRECTORY_MTIME "mtime: "
#define DIRECTORY_BEGIN "begin: "
#define DIRECTORY_END "end: "

gcc_const
static const char *
DeviceToTypeString(unsigned device) noexcept
{
	switch (device) {
	case DEVICE_INARCHIVE:
		return "archive";

	case DEVICE_CONTAINER:
		return "container";

	default:
		return nullptr;
	}
}

gcc_pure
static unsigned
ParseTypeString(const char *type) noexcept
{
	if (strcmp(type, "archive") == 0)
		return DEVICE_INARCHIVE;
	else if (strcmp(type, "container") == 0)
		return DEVICE_CONTAINER;
	else
		return 0;
}

void
directory_save(BufferedOutputStream &os, const Directory &directory)
{
	if (!directory.IsRoot()) {
		const char *type = DeviceToTypeString(directory.device);
		if (type != nullptr)
			os.Format(DIRECTORY_TYPE "%s\n", type);

		if (!IsNegative(directory.mtime))
			os.Format(DIRECTORY_MTIME "%lu\n",
				  (unsigned long)std::chrono::system_clock::to_time_t(directory.mtime));

		os.Format("%s%s\n", DIRECTORY_BEGIN, directory.GetPath());
	}

	for (const auto &child : directory.children) {
		if (child.IsMount())
			continue;

		os.Format(DIRECTORY_DIR "%s\n", child.GetName());
		directory_save(os, child);
	}

	for (const auto &song : directory.songs)
		song_save(os, song);

	playlist_vector_save(os, directory.playlists);

	if (!directory.IsRoot())
		os.Format(DIRECTORY_END "%s\n", directory.GetPath());
}

static bool
ParseLine(Directory &directory, const char *line)
{
	const char *p;
	if ((p = StringAfterPrefix(line, DIRECTORY_MTIME))) {
		const auto mtime = ParseUint64(p);
		if (mtime > 0)
			directory.mtime = std::chrono::system_clock::from_time_t(mtime);
	} else if ((p = StringAfterPrefix(line, DIRECTORY_TYPE))) {
		directory.device = ParseTypeString(p);
	} else
		return false;

	return true;
}

static Directory *
directory_load_subdir(TextFile &file, Directory &parent, const char *name)
{
	if (parent.FindChild(name) != nullptr)
		throw FormatRuntimeError("Duplicate subdirectory '%s'", name);

	Directory *directory = parent.CreateChild(name);

	try {
		while (true) {
			const char *line = file.ReadLine();
			if (line == nullptr)
				throw std::runtime_error("Unexpected end of file");

			if (StringStartsWith(line, DIRECTORY_BEGIN))
				break;

			if (!ParseLine(*directory, line))
				throw FormatRuntimeError("Malformed line: %s", line);
		}

		directory_load(file, *directory);
	} catch (...) {
		directory->Delete();
		throw;
	}

	return directory;
}

void
directory_load(TextFile &file, Directory &directory)
{
	const char *line;

	while ((line = file.ReadLine()) != nullptr &&
	       !StringStartsWith(line, DIRECTORY_END)) {
		const char *p;
		if ((p = StringAfterPrefix(line, DIRECTORY_DIR))) {
			directory_load_subdir(file, directory, p);
		} else if ((p = StringAfterPrefix(line, SONG_BEGIN))) {
			const char *name = p;

			if (directory.FindSong(name) != nullptr)
				throw FormatRuntimeError("Duplicate song '%s'", name);

			auto audio_format = AudioFormat::Undefined();
			auto detached_song = song_load(file, name,
						       &audio_format);

			auto song = Song::NewFrom(std::move(*detached_song),
						  directory);
			song->audio_format = audio_format;

			directory.AddSong(song);
		} else if ((p = StringAfterPrefix(line, PLAYLIST_META_BEGIN))) {
			const char *name = p;
			playlist_metadata_load(file, directory.playlists, name);
		} else {
			throw FormatRuntimeError("Malformed line: %s", line);
		}
	}
}
