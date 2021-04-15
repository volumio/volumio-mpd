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
#include "LocateUri.hxx"
#include "client/Client.hxx"
#include "fs/AllocatedPath.hxx"
#include "ls.hxx"
#include "util/UriUtil.hxx"
#include "util/ASCII.hxx"

#ifdef ENABLE_DATABASE
#include "storage/StorageInterface.hxx"
#endif

#include <stdexcept>

static LocatedUri
LocateFileUri(const char *uri, const Client *client
#ifdef ENABLE_DATABASE
	      , const Storage *storage
#endif
	      )
{
	auto path = AllocatedPath::FromUTF8Throw(uri);

#ifdef ENABLE_DATABASE
	if (storage != nullptr) {
		const char *suffix = storage->MapToRelativeUTF8(uri);
		if (suffix != nullptr)
			/* this path was relative to the music
			   directory */
			return LocatedUri(LocatedUri::Type::RELATIVE, suffix);
	}
#endif

	if (client != nullptr)
		client->AllowFile(path);

	return LocatedUri(LocatedUri::Type::PATH, uri, std::move(path));
}

static LocatedUri
LocateAbsoluteUri(UriPluginKind kind, const char *uri
#ifdef ENABLE_DATABASE
		  , const Storage *storage
#endif
		  )
{
	switch (kind) {
	case UriPluginKind::INPUT:
	case UriPluginKind::STORAGE: // TODO: separate check for storage plugins
		if (!uri_supported_scheme(uri))
			throw std::runtime_error("Unsupported URI scheme");
		break;

	case UriPluginKind::PLAYLIST:
		/* for now, no validation for playlist URIs; this is
		   more complicated because there are three ways to
		   identify which plugin to use: URI scheme, filename
		   suffix and MIME type */
		break;
	}

#ifdef ENABLE_DATABASE
	if (storage != nullptr) {
		const char *suffix = storage->MapToRelativeUTF8(uri);
		if (suffix != nullptr)
			return LocatedUri(LocatedUri::Type::RELATIVE, suffix);
	}
#endif

	return LocatedUri(LocatedUri::Type::ABSOLUTE, uri);
}

LocatedUri
LocateUri(UriPluginKind kind,
	  const char *uri, const Client *client
#ifdef ENABLE_DATABASE
	  , const Storage *storage
#endif
	  )
{
	/* skip the obsolete "file://" prefix */
	const char *path_utf8 = StringAfterPrefixCaseASCII(uri, "file://");
	if (path_utf8 != nullptr) {
		if (!PathTraitsUTF8::IsAbsolute(path_utf8))
			throw std::runtime_error("Malformed file:// URI");

		return LocateFileUri(path_utf8, client
#ifdef ENABLE_DATABASE
				     , storage
#endif
				     );
	} else if (PathTraitsUTF8::IsAbsolute(uri))
		return LocateFileUri(uri, client
#ifdef ENABLE_DATABASE
				     , storage
#endif
				     );
	else if (uri_has_scheme(uri))
		return LocateAbsoluteUri(kind, uri
#ifdef ENABLE_DATABASE
					 , storage
#endif
					 );
	else
		return LocatedUri(LocatedUri::Type::RELATIVE, uri);
}
