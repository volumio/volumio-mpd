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

#include "AdPlugDecoderPlugin.h"
#include "tag/Handler.hxx"
#include "../DecoderAPI.hxx"
#include "CheckAudioFormat.hxx"
#include "fs/Path.hxx"
#include "util/Domain.hxx"
#include "util/Macros.hxx"
#include "Log.hxx"

#include <adplug/adplug.h>
#include <adplug/emuopl.h>

#include <assert.h>

static constexpr Domain adplug_domain("adplug");

static unsigned sample_rate;

static bool
adplug_init(const ConfigBlock &block)
{
	FormatDebug(adplug_domain, "adplug %s",
		    CAdPlug::get_version().c_str());

	sample_rate = block.GetPositiveValue("sample_rate", 48000u);
	CheckSampleRate(sample_rate);

	return true;
}

static void
adplug_file_decode(DecoderClient &client, Path path_fs)
{
	CEmuopl opl(sample_rate, true, true);
	opl.init();

	CPlayer *player = CAdPlug::factory(path_fs.c_str(), &opl);
	if (player == nullptr)
		return;

	const AudioFormat audio_format(sample_rate, SampleFormat::S16, 2);
	assert(audio_format.IsValid());

	client.Ready(audio_format, false,
		     SongTime::FromMS(player->songlength()));

	DecoderCommand cmd;

	do {
		if (!player->update())
			break;

		int16_t buffer[2048];
		constexpr unsigned frames_per_buffer = ARRAY_SIZE(buffer) / 2;
		opl.update(buffer, frames_per_buffer);
		cmd = client.SubmitData(nullptr,
					buffer, sizeof(buffer),
					0);
	} while (cmd == DecoderCommand::NONE);

	delete player;
}

static void
adplug_scan_tag(TagType type, const std::string &value,
		TagHandler &handler) noexcept
{
	if (!value.empty())
		handler.OnTag(type, value.c_str());
}

static bool
adplug_scan_file(Path path_fs, TagHandler &handler) noexcept
{
	CEmuopl opl(sample_rate, true, true);
	opl.init();

	CPlayer *player = CAdPlug::factory(path_fs.c_str(), &opl);
	if (player == nullptr)
		return false;

	handler.OnDuration(SongTime::FromMS(player->songlength()));

	if (handler.WantTag()) {
		adplug_scan_tag(TAG_TITLE, player->gettitle(),
				handler);
		adplug_scan_tag(TAG_ARTIST, player->getauthor(),
				handler);
		adplug_scan_tag(TAG_COMMENT, player->getdesc(),
				handler);
	}

	delete player;
	return true;
}

static const char *const adplug_suffixes[] = {
	"amd",
	"d00",
	"hsc",
	"laa",
	"rad",
	"raw",
	"sa2",
	nullptr
};

const struct DecoderPlugin adplug_decoder_plugin = {
	"adplug",
	adplug_init,
	nullptr,
	nullptr,
	adplug_file_decode,
	adplug_scan_file,
	nullptr,
	nullptr,
	adplug_suffixes,
	nullptr,
};
