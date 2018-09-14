/*
 * Copyright 2003-2017 The Music Player Daemon Project
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
#include "SoftwareMixerPlugin.hxx"
#include "mixer/MixerInternal.hxx"
#include "filter/FilterPlugin.hxx"
#include "filter/FilterRegistry.hxx"
#include "filter/FilterInternal.hxx"
#include "filter/plugins/VolumeFilterPlugin.hxx"
#include "pcm/Volume.hxx"
#include "config/Block.hxx"

#include <assert.h>
#include <math.h>

class SoftwareMixer final : public Mixer {
	Filter *filter = nullptr;

	/**
	 * The current volume in percent (0..100).
	 */
	unsigned volume = 100;

public:
	SoftwareMixer(MixerListener &_listener)
		:Mixer(software_mixer_plugin, _listener)
	{
	}

	void SetFilter(Filter *_filter);

	/* virtual methods from class Mixer */
	void Open() override {
	}

	virtual void Close() override {
	}

	int GetVolume() override {
		return volume;
	}

	void SetVolume(unsigned volume) override;
};

static Mixer *
software_mixer_init(gcc_unused EventLoop &event_loop,
		    gcc_unused AudioOutput &ao,
		    MixerListener &listener,
		    gcc_unused const ConfigBlock &block)
{
	return new SoftwareMixer(listener);
}

gcc_const
static unsigned
PercentVolumeToSoftwareVolume(unsigned volume) noexcept
{
	assert(volume <= 100);

	if (volume >= 100)
		return PCM_VOLUME_1;
	else if (volume > 0)
		return pcm_float_to_volume((exp(volume / 25.0) - 1) /
					   (54.5981500331F - 1));
	else
		return 0;
}

void
SoftwareMixer::SetVolume(unsigned new_volume)
{
	assert(new_volume <= 100);

	volume = new_volume;

	if (filter != nullptr)
		volume_filter_set(filter, PercentVolumeToSoftwareVolume(new_volume));
}

const MixerPlugin software_mixer_plugin = {
	software_mixer_init,
	true,
};

inline void
SoftwareMixer::SetFilter(Filter *_filter)
{
	filter = _filter;

	if (filter != nullptr)
		volume_filter_set(filter,
				  PercentVolumeToSoftwareVolume(volume));
}

void
software_mixer_set_filter(Mixer &mixer, Filter *filter)
{
	SoftwareMixer &sm = (SoftwareMixer &)mixer;
	sm.SetFilter(filter);
}
