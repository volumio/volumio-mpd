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

#include "MultipleOutputs.hxx"
#include "Filtered.hxx"
#include "Defaults.hxx"
#include "Domain.hxx"
#include "MusicPipe.hxx"
#include "MusicChunk.hxx"
#include "filter/Factory.hxx"
#include "config/Block.hxx"
#include "config/Data.hxx"
#include "config/Option.hxx"
#include "util/RuntimeError.hxx"

#include <stdexcept>

#include <assert.h>
#include <string.h>

MultipleOutputs::MultipleOutputs(MixerListener &_mixer_listener) noexcept
	:mixer_listener(_mixer_listener)
{
}

MultipleOutputs::~MultipleOutputs() noexcept
{
	/* parallel destruction */
	for (auto *i : outputs)
		i->BeginDestroy();
	for (auto *i : outputs)
		delete i;
}

static std::unique_ptr<FilteredAudioOutput>
LoadOutput(EventLoop &event_loop,
	   const ReplayGainConfig &replay_gain_config,
	   MixerListener &mixer_listener,
	   const ConfigBlock &block,
	   const AudioOutputDefaults &defaults,
	   FilterFactory *filter_factory)
try {
	return audio_output_new(event_loop, replay_gain_config, block,
				defaults,
				filter_factory,
				mixer_listener);
} catch (...) {
	if (block.line > 0)
		std::throw_with_nested(FormatRuntimeError("Failed to configure output in line %i",
							  block.line));
	else
		throw;
}

static AudioOutputControl *
LoadOutputControl(EventLoop &event_loop,
		  const ReplayGainConfig &replay_gain_config,
		  MixerListener &mixer_listener,
		  AudioOutputClient &client, const ConfigBlock &block,
		  const AudioOutputDefaults &defaults,
		  FilterFactory *filter_factory)
{
	auto output = LoadOutput(event_loop, replay_gain_config,
				 mixer_listener,
				 block, defaults, filter_factory);
	auto *control = new AudioOutputControl(std::move(output), client);

	try {
		control->Configure(block);
	} catch (...) {
		control->BeginDestroy();
		delete control;
		throw;
	}

	return control;
}

void
MultipleOutputs::Configure(EventLoop &event_loop,
			   const ConfigData &config,
			   const ReplayGainConfig &replay_gain_config,
			   AudioOutputClient &client)
{
	const AudioOutputDefaults defaults(config);
	FilterFactory filter_factory(config);

	for (const auto &block : config.GetBlockList(ConfigBlockOption::AUDIO_OUTPUT)) {
		block.SetUsed();
		auto *output = LoadOutputControl(event_loop,
						 replay_gain_config,
						 mixer_listener,
						 client, block, defaults,
						 &filter_factory);
		if (FindByName(output->GetName()) != nullptr)
			throw FormatRuntimeError("output devices with identical "
						 "names: %s", output->GetName());

		outputs.push_back(output);
	}

	if (outputs.empty()) {
		/* auto-detect device */
		const ConfigBlock empty;
		auto *output = LoadOutputControl(event_loop,
						 replay_gain_config,
						 mixer_listener,
						 client, empty, defaults,
						 nullptr);
		outputs.push_back(output);
	}
}

void
MultipleOutputs::AddNullOutput(EventLoop &event_loop,
			       const ReplayGainConfig &replay_gain_config,
			       AudioOutputClient &client)
{
	const AudioOutputDefaults defaults;

	ConfigBlock block;
	block.AddBlockParam("type", "null");

	auto *output = LoadOutputControl(event_loop, replay_gain_config,
					 mixer_listener,
					 client, block, defaults, nullptr);
	outputs.push_back(output);
}

AudioOutputControl *
MultipleOutputs::FindByName(const char *name) noexcept
{
	for (auto *i : outputs)
		if (strcmp(i->GetName(), name) == 0)
			return i;

	return nullptr;
}

void
MultipleOutputs::EnableDisable()
{
	/* parallel execution */

	for (auto *ao : outputs) {
		const std::lock_guard<Mutex> lock(ao->mutex);
		ao->EnableDisableAsync();
	}

	WaitAll();
}

void
MultipleOutputs::WaitAll() noexcept
{
	for (auto *ao : outputs) {
		const std::lock_guard<Mutex> protect(ao->mutex);
		ao->WaitForCommand();
	}
}

void
MultipleOutputs::AllowPlay() noexcept
{
	for (auto *ao : outputs)
		ao->LockAllowPlay();
}

bool
MultipleOutputs::Update(bool force) noexcept
{
	bool ret = false;

	if (!input_audio_format.IsDefined())
		return false;

	for (auto *ao : outputs)
		ret = ao->LockUpdate(input_audio_format, *pipe, force)
			|| ret;

	return ret;
}

void
MultipleOutputs::SetReplayGainMode(ReplayGainMode mode) noexcept
{
	for (auto *ao : outputs)
		ao->SetReplayGainMode(mode);
}

void
MultipleOutputs::Play(MusicChunkPtr chunk)
{
	assert(pipe != nullptr);
	assert(chunk != nullptr);
	assert(chunk->CheckFormat(input_audio_format));

	if (!Update(false))
		/* TODO: obtain real error */
		throw std::runtime_error("Failed to open audio output");

	pipe->Push(std::move(chunk));

	for (auto *ao : outputs)
		ao->LockPlay();
}

void
MultipleOutputs::Open(const AudioFormat audio_format)
{
	bool ret = false, enabled = false;

	/* the audio format must be the same as existing chunks in the
	   pipe */
	assert(pipe == nullptr || pipe->CheckFormat(audio_format));

	if (pipe == nullptr)
		pipe = std::make_unique<MusicPipe>();
	else
		/* if the pipe hasn't been cleared, the the audio
		   format must not have changed */
		assert(pipe->IsEmpty() || audio_format == input_audio_format);

	input_audio_format = audio_format;

	EnableDisable();
	Update(true);

	std::exception_ptr first_error;

	for (auto *ao : outputs) {
		const std::lock_guard<Mutex> lock(ao->mutex);

		if (ao->IsEnabled())
			enabled = true;

		if (ao->IsOpen())
			ret = true;
		else if (!first_error)
			first_error = ao->GetLastError();
	}

	if (!enabled) {
		/* close all devices if there was an error */
		Close();
		throw std::runtime_error("All audio outputs are disabled");
	} else if (!ret) {
		/* close all devices if there was an error */
		Close();

		if (first_error)
			/* we have details, so throw that */
			std::rethrow_exception(first_error);
		else
			throw std::runtime_error("Failed to open audio output");
	}
}

bool
MultipleOutputs::IsChunkConsumed(const MusicChunk *chunk) const noexcept
{
	for (auto *ao : outputs)
		if (!ao->LockIsChunkConsumed(*chunk))
			return false;

	return true;
}

inline void
MultipleOutputs::ClearTailChunk(const MusicChunk *chunk,
				bool *locked) noexcept
{
	assert(chunk->next == nullptr);
	assert(pipe->Contains(chunk));

	for (unsigned i = 0, n = outputs.size(); i != n; ++i) {
		auto *ao = outputs[i];

		/* this mutex will be unlocked by the caller when it's
		   ready */
		ao->mutex.lock();
		locked[i] = ao->IsOpen();

		if (!locked[i]) {
			ao->mutex.unlock();
			continue;
		}

		ao->ClearTailChunk(*chunk);
	}
}

unsigned
MultipleOutputs::CheckPipe() noexcept
{
	const MusicChunk *chunk;
	bool is_tail;
	bool locked[outputs.size()];

	assert(pipe != nullptr);

	while ((chunk = pipe->Peek()) != nullptr) {
		assert(!pipe->IsEmpty());

		if (!IsChunkConsumed(chunk))
			/* at least one output is not finished playing
			   this chunk */
			return pipe->GetSize();

		if (chunk->length > 0 && !chunk->time.IsNegative())
			/* only update elapsed_time if the chunk
			   provides a defined value */
			elapsed_time = chunk->time;

		is_tail = chunk->next == nullptr;
		if (is_tail)
			/* this is the tail of the pipe - clear the
			   chunk reference in all outputs */
			ClearTailChunk(chunk, locked);

		/* remove the chunk from the pipe */
		const auto shifted = pipe->Shift();
		assert(shifted.get() == chunk);

		if (is_tail)
			/* unlock all audio outputs which were locked
			   by clear_tail_chunk() */
			for (unsigned i = 0, n = outputs.size(); i != n; ++i)
				if (locked[i])
					outputs[i]->mutex.unlock();

		/* chunk is automatically returned to the buffer by
		   ~MusicChunkPtr() */
	}

	return 0;
}

void
MultipleOutputs::Pause() noexcept
{
	Update(false);

	for (auto *ao : outputs)
		ao->LockPauseAsync();

	WaitAll();
}

void
MultipleOutputs::Drain() noexcept
{
	for (auto *ao : outputs)
		ao->LockDrainAsync();

	WaitAll();
}

void
MultipleOutputs::Cancel() noexcept
{
	/* send the cancel() command to all audio outputs */

	for (auto *ao : outputs)
		ao->LockCancelAsync();

	WaitAll();

	/* clear the music pipe and return all chunks to the buffer */

	if (pipe != nullptr)
		pipe->Clear();

	/* the audio outputs are now waiting for a signal, to
	   synchronize the cleared music pipe */

	AllowPlay();

	/* invalidate elapsed_time */

	elapsed_time = SignedSongTime::Negative();
}

void
MultipleOutputs::Close() noexcept
{
	for (auto *ao : outputs)
		ao->LockCloseWait();

	pipe.reset();

	input_audio_format.Clear();

	elapsed_time = SignedSongTime::Negative();
}

void
MultipleOutputs::Release() noexcept
{
	for (auto *ao : outputs)
		ao->LockRelease();

	pipe.reset();

	input_audio_format.Clear();

	elapsed_time = SignedSongTime::Negative();
}

void
MultipleOutputs::SongBorder() noexcept
{
	/* clear the elapsed_time pointer at the beginning of a new
	   song */
	elapsed_time = SignedSongTime::zero();
}
