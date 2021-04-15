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

#include "Bridge.hxx"
#include "DecoderAPI.hxx"
#include "Domain.hxx"
#include "Control.hxx"
#include "song/DetachedSong.hxx"
#include "pcm/PcmConvert.hxx"
#include "MusicPipe.hxx"
#include "MusicBuffer.hxx"
#include "MusicChunk.hxx"
#include "pcm/PcmConvert.hxx"
#include "tag/Tag.hxx"
#include "Log.hxx"
#include "input/InputStream.hxx"
#include "util/ConstBuffer.hxx"
#include "util/StringBuffer.hxx"

#include <assert.h>
#include <string.h>
#include <math.h>

DecoderBridge::~DecoderBridge()
{
	/* caller must flush the chunk */
	assert(current_chunk == nullptr);

	if (convert != nullptr) {
		convert->Close();
		delete convert;
	}
}

bool
DecoderBridge::CheckCancelRead() const noexcept
{
	if (error)
		/* this translates to DecoderCommand::STOP */
		return true;

	if (dc.command == DecoderCommand::NONE)
		return false;

	/* ignore the SEEK command during initialization, the plugin
	   should handle that after it has initialized successfully */
	if (dc.command == DecoderCommand::SEEK &&
	    (dc.state == DecoderState::START || seeking ||
	     initial_seek_running))
		return false;

	return true;
}

/**
 * All chunks are full of decoded data; wait for the player to free
 * one.
 */
static DecoderCommand
need_chunks(DecoderControl &dc) noexcept
{
	if (dc.command == DecoderCommand::NONE)
		dc.Wait();

	return dc.command;
}

static DecoderCommand
LockNeedChunks(DecoderControl &dc) noexcept
{
	const std::lock_guard<Mutex> protect(dc.mutex);
	return need_chunks(dc);
}

MusicChunk *
DecoderBridge::GetChunk() noexcept
{
	DecoderCommand cmd;

	if (current_chunk != nullptr)
		return current_chunk.get();

	do {
		current_chunk = dc.buffer->Allocate();
		if (current_chunk != nullptr) {
			current_chunk->replay_gain_serial = replay_gain_serial;
			if (replay_gain_serial != 0)
				current_chunk->replay_gain_info = replay_gain_info;

			return current_chunk.get();
		}

		cmd = LockNeedChunks(dc);
	} while (cmd == DecoderCommand::NONE);

	return nullptr;
}

void
DecoderBridge::FlushChunk()
{
	assert(!seeking);
	assert(!initial_seek_running);
	assert(!initial_seek_pending);
	assert(current_chunk != nullptr);

	auto chunk = std::move(current_chunk);
	if (!chunk->IsEmpty())
		dc.pipe->Push(std::move(chunk));

	const std::lock_guard<Mutex> protect(dc.mutex);
	if (dc.client_is_waiting)
		dc.client_cond.signal();
}

bool
DecoderBridge::PrepareInitialSeek()
{
	assert(dc.pipe != nullptr);

	if (dc.state != DecoderState::DECODE)
		/* wait until the decoder has finished initialisation
		   (reading file headers etc.) before emitting the
		   virtual "SEEK" command */
		return false;

	if (initial_seek_running)
		/* initial seek has already begun - override any other
		   command */
		return true;

	if (initial_seek_pending) {
		if (!dc.seekable) {
			/* seeking is not possible */
			initial_seek_pending = false;
			return false;
		}

		if (dc.command == DecoderCommand::NONE) {
			/* begin initial seek */

			initial_seek_pending = false;
			initial_seek_running = true;
			return true;
		}

		/* skip initial seek when there's another command
		   (e.g. STOP) */

		initial_seek_pending = false;
	}

	return false;
}

DecoderCommand
DecoderBridge::GetVirtualCommand() noexcept
{
	if (error)
		/* an error has occurred: stop the decoder plugin */
		return DecoderCommand::STOP;

	assert(dc.pipe != nullptr);

	if (PrepareInitialSeek())
		return DecoderCommand::SEEK;

	return dc.command;
}

DecoderCommand
DecoderBridge::LockGetVirtualCommand() noexcept
{
	const std::lock_guard<Mutex> protect(dc.mutex);
	return GetVirtualCommand();
}

DecoderCommand
DecoderBridge::DoSendTag(const Tag &tag)
{
	if (current_chunk != nullptr) {
		/* there is a partial chunk - flush it, we want the
		   tag in a new chunk */
		FlushChunk();
	}

	assert(current_chunk == nullptr);

	auto *chunk = GetChunk();
	if (chunk == nullptr) {
		assert(dc.command != DecoderCommand::NONE);
		return dc.command;
	}

	chunk->tag = std::make_unique<Tag>(tag);
	return DecoderCommand::NONE;
}

bool
DecoderBridge::UpdateStreamTag(InputStream *is)
{
	auto tag = is != nullptr
		? is->LockReadTag()
		: nullptr;
	if (tag == nullptr) {
		tag = std::move(song_tag);
		if (tag == nullptr)
			return false;

		/* no stream tag present - submit the song tag
		   instead */
	} else
		/* discard the song tag; we don't need it */
		song_tag.reset();

	stream_tag = std::move(tag);
	return true;
}

void
DecoderBridge::Ready(const AudioFormat audio_format,
		     bool seekable, SignedSongTime duration)
{
	assert(convert == nullptr);
	assert(stream_tag == nullptr);
	assert(decoder_tag == nullptr);
	assert(!seeking);

	FormatDebug(decoder_domain, "audio_format=%s, seekable=%s",
		    ToString(audio_format).c_str(),
		    seekable ? "true" : "false");

	{
		const std::lock_guard<Mutex> protect(dc.mutex);
		dc.SetReady(audio_format, seekable, duration);
	}

	if (dc.in_audio_format != dc.out_audio_format) {
		FormatDebug(decoder_domain, "converting to %s",
			    ToString(dc.out_audio_format).c_str());

		convert = new PcmConvert();

		try {
			convert->Open(dc.in_audio_format,
					      dc.out_audio_format);
		} catch (...) {
			error = std::current_exception();
		}
	}
}

DecoderCommand
DecoderBridge::GetCommand() noexcept
{
	return LockGetVirtualCommand();
}

void
DecoderBridge::CommandFinished()
{
	const std::lock_guard<Mutex> protect(dc.mutex);

	assert(dc.command != DecoderCommand::NONE || initial_seek_running);
	assert(dc.command != DecoderCommand::SEEK ||
	       initial_seek_running ||
	       dc.seek_error || seeking);
	assert(dc.pipe != nullptr);

	if (initial_seek_running) {
		assert(!seeking);
		assert(current_chunk == nullptr);
		assert(dc.pipe->IsEmpty());

		initial_seek_running = false;
		timestamp = std::chrono::duration_cast<FloatDuration>(dc.start_time);
		absolute_frame = dc.start_time.ToScale<uint64_t>(dc.in_audio_format.sample_rate);
		return;
	}

	if (seeking) {
		seeking = false;

		/* delete frames from the old song position */

		current_chunk.reset();

		dc.pipe->Clear();

		if (convert != nullptr)
			convert->Reset();

		timestamp = std::chrono::duration_cast<FloatDuration>(dc.seek_time);
		absolute_frame = dc.seek_time.ToScale<uint64_t>(dc.in_audio_format.sample_rate);
	}

	dc.command = DecoderCommand::NONE;
	dc.client_cond.signal();
}

SongTime
DecoderBridge::GetSeekTime() noexcept
{
	assert(dc.pipe != nullptr);

	if (initial_seek_running)
		return dc.start_time;

	assert(dc.command == DecoderCommand::SEEK);

	seeking = true;

	return dc.seek_time;
}

uint64_t
DecoderBridge::GetSeekFrame() noexcept
{
	return GetSeekTime().ToScale<uint64_t>(dc.in_audio_format.sample_rate);
}

void
DecoderBridge::SeekError()
{
	assert(dc.pipe != nullptr);

	if (initial_seek_running) {
		/* d'oh, we can't seek to the sub-song start position,
		   what now? - no idea, ignoring the problem for now. */
		initial_seek_running = false;
		return;
	}

	assert(dc.command == DecoderCommand::SEEK);

	dc.seek_error = true;
	seeking = false;

	CommandFinished();
}

InputStreamPtr
DecoderBridge::OpenUri(const char *uri)
{
	assert(dc.state == DecoderState::START ||
	       dc.state == DecoderState::DECODE);

	Mutex &mutex = dc.mutex;
	Cond &cond = dc.cond;

	auto is = InputStream::Open(uri, mutex);
	is->SetHandler(&dc);

	const std::lock_guard<Mutex> lock(mutex);
	while (true) {
		is->Update();
		if (is->IsReady())
			return is;

		if (dc.command == DecoderCommand::STOP)
			throw StopDecoder();

		cond.wait(mutex);
	}
}

size_t
DecoderBridge::Read(InputStream &is, void *buffer, size_t length)
try {
	assert(buffer != nullptr);
	assert(dc.state == DecoderState::START ||
	       dc.state == DecoderState::DECODE);

	if (length == 0)
		return 0;

	std::lock_guard<Mutex> lock(is.mutex);

	while (true) {
		if (CheckCancelRead())
			return 0;

		if (is.IsAvailable())
			break;

		dc.cond.wait(is.mutex);
	}

	size_t nbytes = is.Read(buffer, length);
	assert(nbytes > 0 || is.IsEOF());

	return nbytes;
} catch (...) {
	error = std::current_exception();
	return 0;
}

void
DecoderBridge::SubmitTimestamp(FloatDuration t)
{
	assert(t.count() >= 0);

	timestamp = t;
	absolute_frame = uint64_t(t.count() * dc.in_audio_format.sample_rate);
}

DecoderCommand
DecoderBridge::SubmitData(InputStream *is,
			  const void *data, size_t length,
			  uint16_t kbit_rate)
{
	assert(dc.state == DecoderState::DECODE);
	assert(dc.pipe != nullptr);
	assert(length % dc.in_audio_format.GetFrameSize() == 0);

	DecoderCommand cmd = LockGetVirtualCommand();

	if (cmd == DecoderCommand::STOP || cmd == DecoderCommand::SEEK ||
	    length == 0)
		return cmd;

	assert(!initial_seek_pending);
	assert(!initial_seek_running);

	/* send stream tags */

	if (UpdateStreamTag(is)) {
		if (decoder_tag != nullptr)
			/* merge with tag from decoder plugin */
			cmd = DoSendTag(*Tag::Merge(*decoder_tag,
						    *stream_tag));
		else
			/* send only the stream tag */
			cmd = DoSendTag(*stream_tag);

		if (cmd != DecoderCommand::NONE)
			return cmd;
	}

	cmd = DecoderCommand::NONE;

	const size_t frame_size = dc.in_audio_format.GetFrameSize();
	size_t data_frames = length / frame_size;

	if (dc.end_time.IsPositive()) {
		/* enforce the given end time */

		const uint64_t end_frame =
			dc.end_time.ToScale<uint64_t>(dc.in_audio_format.sample_rate);
		if (absolute_frame >= end_frame)
			return DecoderCommand::STOP;

		const uint64_t remaining_frames = end_frame - absolute_frame;
		if (data_frames >= remaining_frames) {
			/* past the end of the range: truncate this
			   data submission and stop the decoder */
			data_frames = remaining_frames;
			length = data_frames * frame_size;
			cmd = DecoderCommand::STOP;
		}
	}

	if (convert != nullptr) {
		assert(dc.in_audio_format != dc.out_audio_format);

		try {
			auto result = convert->Convert({data, length});
			data = result.data;
			length = result.size;
		} catch (...) {
			/* the PCM conversion has failed - stop
			   playback, since we have no better way to
			   bail out */
			error = std::current_exception();
			return DecoderCommand::STOP;
		}
	} else {
		assert(dc.in_audio_format == dc.out_audio_format);
	}

	while (length > 0) {
		bool full;

		auto *chunk = GetChunk();
		if (chunk == nullptr) {
			assert(dc.command != DecoderCommand::NONE);
			return dc.command;
		}

		const auto dest =
			chunk->Write(dc.out_audio_format,
				     SongTime::Cast(timestamp) -
				     dc.song->GetStartTime(),
				     kbit_rate);
		if (dest.empty()) {
			/* the chunk is full, flush it */
			FlushChunk();
			continue;
		}

		const size_t nbytes = std::min(dest.size, length);

		/* copy the buffer */

		memcpy(dest.data, data, nbytes);

		/* expand the music pipe chunk */

		full = chunk->Expand(dc.out_audio_format, nbytes);
		if (full) {
			/* the chunk is full, flush it */
			FlushChunk();
		}

		data = (const uint8_t *)data + nbytes;
		length -= nbytes;

		timestamp += dc.out_audio_format.SizeToTime<FloatDuration>(nbytes);
	}

	absolute_frame += data_frames;

	return cmd;
}

DecoderCommand
DecoderBridge::SubmitTag(InputStream *is, Tag &&tag)
{
	DecoderCommand cmd;

	assert(dc.state == DecoderState::DECODE);
	assert(dc.pipe != nullptr);

	/* save the tag */

	decoder_tag = std::make_unique<Tag>(std::move(tag));

	/* check for a new stream tag */

	UpdateStreamTag(is);

	/* check if we're seeking */

	if (PrepareInitialSeek())
		/* during initial seek, no music chunk must be created
		   until seeking is finished; skip the rest of the
		   function here */
		return DecoderCommand::SEEK;

	/* send tag to music pipe */

	if (stream_tag != nullptr)
		/* merge with tag from input stream */
		cmd = DoSendTag(*Tag::Merge(*stream_tag, *decoder_tag));
	else
		/* send only the decoder tag */
		cmd = DoSendTag(*decoder_tag);

	return cmd;
}

void
DecoderBridge::SubmitReplayGain(const ReplayGainInfo *new_replay_gain_info)
{
	if (new_replay_gain_info != nullptr) {
		static unsigned serial;
		if (++serial == 0)
			serial = 1;

		if (ReplayGainMode::OFF != dc.replay_gain_mode) {
			ReplayGainMode rgm = dc.replay_gain_mode;
			if (rgm != ReplayGainMode::ALBUM)
				rgm = ReplayGainMode::TRACK;

			const auto &tuple = new_replay_gain_info->Get(rgm);
			const auto scale =
				tuple.CalculateScale(dc.replay_gain_config);
			dc.replay_gain_db = 20.0 * log10f(scale);
		}

		replay_gain_info = *new_replay_gain_info;
		replay_gain_serial = serial;

		if (current_chunk != nullptr) {
			/* flush the current chunk because the new
			   replay gain values affect the following
			   samples */
			FlushChunk();
		}
	} else
		replay_gain_serial = 0;
}

void
DecoderBridge::SubmitMixRamp(MixRampInfo &&mix_ramp)
{
	dc.SetMixRamp(std::move(mix_ramp));
}
