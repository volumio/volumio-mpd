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

#include "ThreadInputStream.hxx"
#include "CondHandler.hxx"
#include "thread/Name.hxx"

#include <assert.h>
#include <string.h>

ThreadInputStream::ThreadInputStream(const char *_plugin,
				     const char *_uri,
				     Mutex &_mutex,
				     size_t _buffer_size) noexcept
	:InputStream(_uri, _mutex),
	 plugin(_plugin),
	 thread(BIND_THIS_METHOD(ThreadFunc)),
	 allocation(_buffer_size),
	 buffer(&allocation.front(), allocation.size())
{
	allocation.ForkCow(false);
}

void
ThreadInputStream::Stop() noexcept
{
	if (!thread.IsDefined())
		return;

	{
		const std::lock_guard<Mutex> lock(mutex);
		close = true;
		wake_cond.signal();
	}

	Cancel();

	thread.Join();

	buffer.Clear();
}

void
ThreadInputStream::Start()
{
	thread.Start();
}

inline void
ThreadInputStream::ThreadFunc() noexcept
{
	FormatThreadName("input:%s", plugin);

	const std::lock_guard<Mutex> lock(mutex);

	try {
		Open();
	} catch (...) {
		postponed_exception = std::current_exception();
		SetReady();
		return;
	}

	/* we're ready, tell it to our client */
	SetReady();

	while (!close) {
		assert(!postponed_exception);

		auto w = buffer.Write();
		if (w.empty()) {
			wake_cond.wait(mutex);
		} else {
			size_t nbytes;

			try {
				const ScopeUnlock unlock(mutex);
				nbytes = ThreadRead(w.data, w.size);
			} catch (...) {
				postponed_exception = std::current_exception();
				InvokeOnAvailable();
				break;
			}

			InvokeOnAvailable();

			if (nbytes == 0) {
				eof = true;
				break;
			}

			buffer.Append(nbytes);
		}
	}

	Close();
}

void
ThreadInputStream::Check()
{
	assert(!thread.IsInside());

	if (postponed_exception)
		std::rethrow_exception(postponed_exception);
}

bool
ThreadInputStream::IsAvailable() noexcept
{
	assert(!thread.IsInside());

	return !buffer.empty() || eof || postponed_exception;
}

inline size_t
ThreadInputStream::Read(void *ptr, size_t read_size)
{
	assert(!thread.IsInside());

	CondInputStreamHandler cond_handler;

	while (true) {
		if (postponed_exception)
			std::rethrow_exception(postponed_exception);

		auto r = buffer.Read();
		if (!r.empty()) {
			size_t nbytes = std::min(read_size, r.size);
			memcpy(ptr, r.data, nbytes);
			buffer.Consume(nbytes);
			wake_cond.broadcast();
			offset += nbytes;
			return nbytes;
		}

		if (eof)
			return 0;

		const ScopeExchangeInputStreamHandler h(*this, &cond_handler);
		cond_handler.cond.wait(mutex);
	}
}

bool
ThreadInputStream::IsEOF() noexcept
{
	assert(!thread.IsInside());

	return eof;
}
