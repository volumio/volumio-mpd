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

#include "Walk.hxx"
#include "UpdateDomain.hxx"
#include "db/DatabaseLock.hxx"
#include "db/plugins/simple/Directory.hxx"
#include "db/plugins/simple/Song.hxx"
#include "storage/StorageInterface.hxx"
#include "fs/AllocatedPath.hxx"
#include "storage/FileInfo.hxx"
#include "archive/ArchiveList.hxx"
#include "archive/ArchivePlugin.hxx"
#include "archive/ArchiveFile.hxx"
#include "archive/ArchiveVisitor.hxx"
#include "util/StringCompare.hxx"
#include "Log.hxx"

#include <string>
#include <exception>

#include <string.h>

static Directory *
LockFindChild(Directory &directory, const char *name) noexcept
{
	const ScopeDatabaseLock protect;
	return directory.FindChild(name);
}

static Directory *
LockMakeChild(Directory &directory, const char *name) noexcept
{
	const ScopeDatabaseLock protect;
	return directory.MakeChild(name);
}

static Song *
LockFindSong(Directory &directory, const char *name) noexcept
{
	const ScopeDatabaseLock protect;
	return directory.FindSong(name);
}

void
UpdateWalk::UpdateArchiveTree(ArchiveFile &archive, Directory &directory,
			      const char *name) noexcept
{
	const char *tmp = strchr(name, '/');
	if (tmp) {
		const std::string child_name(name, tmp);
		//add dir is not there already
		Directory *subdir = LockMakeChild(directory,
						  child_name.c_str());
		subdir->device = DEVICE_INARCHIVE;

		//create directories first
		UpdateArchiveTree(archive, *subdir, tmp + 1);
	} else {
		if (StringIsEmpty(name)) {
			LogWarning(update_domain,
				   "archive returned directory only");
			return;
		}

		//add file
		Song *song = LockFindSong(directory, name);
		if (song == nullptr) {
			song = Song::LoadFromArchive(archive, name, directory);
			if (song != nullptr) {
				{
					const ScopeDatabaseLock protect;
					directory.AddSong(song);
				}

				modified = true;
				FormatDefault(update_domain, "added %s/%s",
					      directory.GetPath(), name);
			}
		} else {
			if (!song->UpdateFileInArchive(archive)) {
				FormatDebug(update_domain,
					    "deleting unrecognized file %s/%s",
					    directory.GetPath(), name);
				editor.LockDeleteSong(directory, song);
			}
		}
	}
}

class UpdateArchiveVisitor final : public ArchiveVisitor {
	UpdateWalk &walk;
	ArchiveFile &archive;
	Directory *directory;

 public:
	UpdateArchiveVisitor(UpdateWalk &_walk, ArchiveFile &_archive,
			     Directory *_directory) noexcept
		:walk(_walk), archive(_archive), directory(_directory) {}

	virtual void VisitArchiveEntry(const char *path_utf8) override {
		FormatDebug(update_domain,
			    "adding archive file: %s", path_utf8);
		walk.UpdateArchiveTree(archive, *directory, path_utf8);
	}
};

/**
 * Updates the file listing from an archive file.
 *
 * @param parent the parent directory the archive file resides in
 * @param name the UTF-8 encoded base name of the archive file
 * @param st stat() information on the archive file
 * @param plugin the archive plugin which fits this archive type
 */
void
UpdateWalk::UpdateArchiveFile(Directory &parent, const char *name,
			      const StorageFileInfo &info,
			      const ArchivePlugin &plugin) noexcept
{
	Directory *directory = LockFindChild(parent, name);

	if (directory != nullptr && directory->mtime == info.mtime &&
	    !walk_discard)
		/* MPD has already scanned the archive, and it hasn't
		   changed since - don't consider updating it */
		return;

	const auto path_fs = storage.MapChildFS(parent.GetPath(), name);
	if (path_fs.IsNull())
		/* not a local file: skip, because the archive API
		   supports only local files */
		return;

	/* open archive */
	std::unique_ptr<ArchiveFile> file;
	try {
		file = archive_file_open(&plugin, path_fs);
	} catch (...) {
		LogError(std::current_exception());
		if (directory != nullptr)
			editor.LockDeleteDirectory(directory);
		return;
	}

	FormatDebug(update_domain, "archive %s opened", path_fs.c_str());

	if (directory == nullptr) {
		FormatDebug(update_domain,
			    "creating archive directory: %s", name);

		const ScopeDatabaseLock protect;
		directory = parent.CreateChild(name);
		/* mark this directory as archive (we use device for
		   this) */
		directory->device = DEVICE_INARCHIVE;
	}

	directory->mtime = info.mtime;

	UpdateArchiveVisitor visitor(*this, *file, directory);
	file->Visit(visitor);
}

bool
UpdateWalk::UpdateArchiveFile(Directory &directory,
			      const char *name, const char *suffix,
			      const StorageFileInfo &info) noexcept
{
	const ArchivePlugin *plugin = archive_plugin_from_suffix(suffix);
	if (plugin == nullptr)
		return false;

	UpdateArchiveFile(directory, name, info, *plugin);
	return true;
}
