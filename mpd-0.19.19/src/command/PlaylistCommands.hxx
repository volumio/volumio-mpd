/*
 * Copyright (C) 2003-2014 The Music Player Daemon Project
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

#ifndef MPD_PLAYLIST_COMMANDS_HXX
#define MPD_PLAYLIST_COMMANDS_HXX

#include "CommandResult.hxx"

class Client;

CommandResult
handle_save(Client &client, unsigned argc, char *argv[]);

CommandResult
handle_load(Client &client, unsigned argc, char *argv[]);

CommandResult
handle_listplaylist(Client &client, unsigned argc, char *argv[]);

CommandResult
handle_listplaylistinfo(Client &client, unsigned argc, char *argv[]);

CommandResult
handle_rm(Client &client, unsigned argc, char *argv[]);

CommandResult
handle_rename(Client &client, unsigned argc, char *argv[]);

CommandResult
handle_playlistdelete(Client &client, unsigned argc, char *argv[]);

CommandResult
handle_playlistmove(Client &client, unsigned argc, char *argv[]);

CommandResult
handle_playlistclear(Client &client, unsigned argc, char *argv[]);

CommandResult
handle_playlistadd(Client &client, unsigned argc, char *argv[]);

CommandResult
handle_listplaylists(Client &client, unsigned argc, char *argv[]);

#endif
