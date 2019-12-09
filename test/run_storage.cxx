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

#include "event/Thread.hxx"
#include "storage/Registry.hxx"
#include "storage/StorageInterface.hxx"
#include "storage/FileInfo.hxx"
#include "net/Init.hxx"
#include "util/ChronoUtil.hxx"
#include "util/PrintException.hxx"

#include <memory>
#include <stdexcept>

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

static std::unique_ptr<Storage>
MakeStorage(EventLoop &event_loop, const char *uri)
{
	auto storage = CreateStorageURI(event_loop, uri);
	if (storage == nullptr)
		throw std::runtime_error("Unrecognized storage URI");

	return storage;
}

static int
Ls(Storage &storage, const char *path)
{
	auto dir = storage.OpenDirectory(path);

	const char *name;
	while ((name = dir->Read()) != nullptr) {
		const auto info = dir->GetInfo(false);

		const char *type = "unk";
		switch (info.type) {
		case StorageFileInfo::Type::OTHER:
			type = "oth";
			break;

		case StorageFileInfo::Type::REGULAR:
			type = "reg";
			break;

		case StorageFileInfo::Type::DIRECTORY:
			type = "dir";
			break;
		}

		char mtime_buffer[32];
		const char *mtime = "          ";
		if (!IsNegative(info.mtime)) {
			time_t t = std::chrono::system_clock::to_time_t(info.mtime);
			strftime(mtime_buffer, sizeof(mtime_buffer),
#ifdef _WIN32
				 "%Y-%m-%d",
#else
				 "%F",
#endif
				 gmtime(&t));
			mtime = mtime_buffer;
		}

		printf("%s %10llu %s %s\n",
		       type, (unsigned long long)info.size,
		       mtime, name);
	}

	return EXIT_SUCCESS;
}

int
main(int argc, char **argv)
try {
	if (argc < 3) {
		fprintf(stderr, "Usage: run_storage COMMAND URI ...\n");
		return EXIT_FAILURE;
	}

	const char *const command = argv[1];
	const char *const storage_uri = argv[2];

	const ScopeNetInit net_init;
	EventThread io_thread;
	io_thread.Start();

	if (strcmp(command, "ls") == 0) {
		if (argc != 4) {
			fprintf(stderr, "Usage: run_storage ls URI PATH\n");
			return EXIT_FAILURE;
		}

		const char *const path = argv[3];

		auto storage = MakeStorage(io_thread.GetEventLoop(),
					   storage_uri);

		return Ls(*storage, path);
	} else {
		fprintf(stderr, "Unknown command\n");
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
} catch (...) {
	PrintException(std::current_exception());
	return EXIT_FAILURE;
}
