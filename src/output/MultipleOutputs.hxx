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

/*
 * Functions for dealing with all configured (enabled) audion outputs
 * at once.
 *
 */

#ifndef OUTPUT_ALL_H
#define OUTPUT_ALL_H

#include "Control.hxx"
#include "MusicChunkPtr.hxx"
#include "player/Outputs.hxx"
#include "AudioFormat.hxx"
#include "ReplayGainMode.hxx"
#include "Chrono.hxx"
#include "util/Compiler.h"

#include <vector>

#include <assert.h>

class MusicPipe;
class EventLoop;
class MixerListener;
class AudioOutputClient;
struct ConfigData;
struct ReplayGainConfig;

class MultipleOutputs final : public PlayerOutputs {
	MixerListener &mixer_listener;

	std::vector<AudioOutputControl *> outputs;

	AudioFormat input_audio_format = AudioFormat::Undefined();

	/**
	 * The #MusicPipe object which feeds all audio outputs.  It is
	 * filled by Play().
	 */
	std::unique_ptr<MusicPipe> pipe;

	/**
	 * The "elapsed_time" stamp of the most recently finished
	 * chunk.
	 */
	SignedSongTime elapsed_time = SignedSongTime::Negative();

public:
	/**
	 * Load audio outputs from the configuration file and
	 * initialize them.
	 */
	MultipleOutputs(MixerListener &_mixer_listener) noexcept;
	~MultipleOutputs() noexcept;

	void Configure(EventLoop &event_loop,
		       const ConfigData &config,
		       const ReplayGainConfig &replay_gain_config,
		       AudioOutputClient &client);

	void AddNullOutput(EventLoop &event_loop,
			   const ReplayGainConfig &replay_gain_config,
			   AudioOutputClient &client);

	/**
	 * Returns the total number of audio output devices, including
	 * those which are disabled right now.
	 */
	gcc_pure
	unsigned Size() const noexcept {
		return outputs.size();
	}

	/**
	 * Returns the "i"th audio output device.
	 */
	const AudioOutputControl &Get(unsigned i) const noexcept {
		assert(i < Size());

		return *outputs[i];
	}

	AudioOutputControl &Get(unsigned i) noexcept {
		assert(i < Size());

		return *outputs[i];
	}

	/**
	 * Returns the audio output device with the specified name.
	 * Returns nullptr if the name does not exist.
	 */
	gcc_pure
	AudioOutputControl *FindByName(const char *name) noexcept;

	void SetReplayGainMode(ReplayGainMode mode) noexcept;

	/**
	 * Returns the average volume of all available mixers (range
	 * 0..100).  Returns -1 if no mixer can be queried.
	 */
	gcc_pure
	int GetVolume() const noexcept;

	/**
	 * Sets the volume on all available mixers.
	 *
	 * @param volume the volume (range 0..100)
	 * @return true on success, false on failure
	 */
	bool SetVolume(unsigned volume) noexcept;

	/**
	 * Similar to GetVolume(), but gets the volume only for
	 * software mixers.  See #software_mixer_plugin.  This
	 * function fails if no software mixer is configured.
	 */
	gcc_pure
	int GetSoftwareVolume() const noexcept;

	/**
	 * Similar to SetVolume(), but sets the volume only for
	 * software mixers.  See #software_mixer_plugin.  This
	 * function cannot fail, because the underlying software
	 * mixers cannot fail either.
	 */
	void SetSoftwareVolume(unsigned volume) noexcept;

private:
	/**
	 * Wait until all (active) outputs have finished the current
	 * command.
	 */
	void WaitAll() noexcept;

	/**
	 * Signals all audio outputs which are open.
	 */
	void AllowPlay() noexcept;

	/**
	 * Opens all output devices which are enabled, but closed.
	 *
	 * @return true if there is at least open output device which
	 * is open
	 */
	bool Update(bool force) noexcept;

	/**
	 * Has this chunk been consumed by all audio outputs?
	 */
	bool IsChunkConsumed(const MusicChunk *chunk) const noexcept;

	/**
	 * There's only one chunk left in the pipe (#pipe), and all
	 * audio outputs have consumed it already.  Clear the
	 * reference.
	 */
	void ClearTailChunk(const MusicChunk *chunk, bool *locked) noexcept;

	/* virtual methods from class PlayerOutputs */
	void EnableDisable() override;
	void Open(const AudioFormat audio_format) override;
	void Close() noexcept override;
	void Release() noexcept override;
	void Play(MusicChunkPtr chunk) override;
	unsigned CheckPipe() noexcept override;
	void Pause() noexcept override;
	void Drain() noexcept override;
	void Cancel() noexcept override;
	void SongBorder() noexcept override;
	SignedSongTime GetElapsedTime() const noexcept override {
		return elapsed_time;
	}
};

#endif
