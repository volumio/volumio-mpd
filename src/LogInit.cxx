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
#include "LogInit.hxx"
#include "LogBackend.hxx"
#include "Log.hxx"
#include "config/Param.hxx"
#include "config/Data.hxx"
#include "config/Option.hxx"
#include "fs/AllocatedPath.hxx"
#include "fs/FileSystem.hxx"
#include "util/Domain.hxx"
#include "util/RuntimeError.hxx"
#include "system/Error.hxx"

#ifdef ENABLE_SYSTEMD_DAEMON
#include <systemd/sd-daemon.h>
#endif

#include <assert.h>
#include <string.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>

#define LOG_LEVEL_SECURE LogLevel::INFO

#define LOG_DATE_BUF_SIZE 16
#define LOG_DATE_LEN (LOG_DATE_BUF_SIZE - 1)

gcc_unused
static constexpr Domain log_domain("log");

#ifndef ANDROID

static int out_fd = -1;
static AllocatedPath out_path = nullptr;

static void redirect_logs(int fd)
{
	assert(fd >= 0);
	if (dup2(fd, STDOUT_FILENO) < 0)
		throw MakeErrno("Failed to dup2 stdout");
	if (dup2(fd, STDERR_FILENO) < 0)
		throw MakeErrno("Failed to dup2 stderr");
}

static int
open_log_file(void)
{
	assert(!out_path.IsNull());

	return OpenFile(out_path, O_CREAT | O_WRONLY | O_APPEND, 0666).Steal();
}

static void
log_init_file(int line)
{
	assert(!out_path.IsNull());

	out_fd = open_log_file();
	if (out_fd < 0) {
#ifdef _WIN32
		const std::string out_path_utf8 = out_path.ToUTF8();
		throw FormatRuntimeError("failed to open log file \"%s\" (config line %d)",
					 out_path_utf8.c_str(), line);
#else
		int e = errno;
		const std::string out_path_utf8 = out_path.ToUTF8();
		throw FormatErrno(e, "failed to open log file \"%s\" (config line %d)",
				  out_path_utf8.c_str(), line);
#endif
	}

	EnableLogTimestamp();
}

static inline LogLevel
parse_log_level(const char *value, int line)
{
	if (0 == strcmp(value, "default"))
		return LogLevel::DEFAULT;
	if (0 == strcmp(value, "secure"))
		return LOG_LEVEL_SECURE;
	else if (0 == strcmp(value, "verbose"))
		return LogLevel::DEBUG;
	else
		throw FormatRuntimeError("unknown log level \"%s\" at line %d",
					 value, line);
}

#endif

void
log_early_init(bool verbose) noexcept
{
#ifdef ANDROID
	(void)verbose;
#else
	/* force stderr to be line-buffered */
	setvbuf(stderr, nullptr, _IOLBF, 0);

	if (verbose)
		SetLogThreshold(LogLevel::DEBUG);
#endif
}

void
log_init(const ConfigData &config, bool verbose, bool use_stdout)
{
#ifdef ANDROID
	(void)config;
	(void)verbose;
	(void)use_stdout;
#else
	if (verbose)
		SetLogThreshold(LogLevel::DEBUG);
	else if (const auto &param = config.GetParam(ConfigOption::LOG_LEVEL))
		SetLogThreshold(parse_log_level(param->value.c_str(),
						param->line));

	if (use_stdout) {
		out_fd = STDOUT_FILENO;
	} else {
		const auto *param = config.GetParam(ConfigOption::LOG_FILE);
		if (param == nullptr) {
			/* no configuration: default to syslog (if
			   available) */
#ifdef ENABLE_SYSTEMD_DAEMON
			if (sd_booted() &&
			    getenv("NOTIFY_SOCKET") != nullptr) {
				/* if MPD was started as a systemd
				   service, default to journal (which
				   is connected to fd=2) */
				out_fd = STDOUT_FILENO;
				return;
			}
#endif
#ifndef HAVE_SYSLOG
			throw std::runtime_error("config parameter 'log_file' not found");
#endif
#ifdef HAVE_SYSLOG
		} else if (strcmp(param->value.c_str(), "syslog") == 0) {
			LogInitSysLog();
#endif
		} else {
			out_path = param->GetPath();
			log_init_file(param->line);
		}
	}
#endif
}

#ifndef ANDROID

static void
close_log_files() noexcept
{
#ifdef HAVE_SYSLOG
	LogFinishSysLog();
#endif
}

#endif

void
log_deinit() noexcept
{
#ifndef ANDROID
	close_log_files();
	out_path = nullptr;
#endif
}

void setup_log_output()
{
#ifndef ANDROID
	if (out_fd == STDOUT_FILENO)
		return;

	fflush(nullptr);

	if (out_fd < 0) {
#ifdef _WIN32
		return;
#else
		out_fd = open("/dev/null", O_WRONLY);
		if (out_fd < 0)
			return;
#endif
	}

	redirect_logs(out_fd);
	close(out_fd);
	out_fd = -1;
#endif
}

int
cycle_log_files() noexcept
{
#ifdef ANDROID
	return 0;
#else
	int fd;

	if (out_path.IsNull())
		return 0;

	FormatDebug(log_domain, "Cycling log files");
	close_log_files();

	fd = open_log_file();
	if (fd < 0) {
		const std::string out_path_utf8 = out_path.ToUTF8();
		FormatError(log_domain,
			    "error re-opening log file: %s",
			    out_path_utf8.c_str());
		return -1;
	}

	redirect_logs(fd);
	close(fd);

	FormatDebug(log_domain, "Done cycling log files");
	return 0;
#endif
}
