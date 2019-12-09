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

#include "SignalHandlers.hxx"
#include "event/SignalMonitor.hxx"

#ifndef _WIN32

#include "Log.hxx"
#include "LogInit.hxx"
#include "event/Loop.hxx"
#include "system/Error.hxx"
#include "util/Domain.hxx"

#include <signal.h>

static constexpr Domain signal_handlers_domain("signal_handlers");

static void
HandleShutdownSignal(void *ctx) noexcept
{
	auto &loop = *(EventLoop *)ctx;
	loop.Break();
}

static void
x_sigaction(int signum, const struct sigaction *act)
{
	if (sigaction(signum, act, NULL) < 0)
		throw MakeErrno("sigaction() failed");
}

static void
handle_reload_event(void *) noexcept
{
	LogDebug(signal_handlers_domain, "got SIGHUP, reopening log files");
	cycle_log_files();
}

#endif

void
SignalHandlersInit(EventLoop &loop)
{
	SignalMonitorInit(loop);

#ifndef _WIN32
	struct sigaction sa;

	sa.sa_flags = 0;
	sigemptyset(&sa.sa_mask);
	sa.sa_handler = SIG_IGN;
	x_sigaction(SIGPIPE, &sa);

	SignalMonitorRegister(SIGINT, {&loop, HandleShutdownSignal});
	SignalMonitorRegister(SIGTERM, {&loop, HandleShutdownSignal});

	SignalMonitorRegister(SIGHUP, {nullptr, handle_reload_event});
#endif
}

void
SignalHandlersFinish() noexcept
{
	SignalMonitorFinish();
}
