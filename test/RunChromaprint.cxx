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

#include "ConfigGlue.hxx"
#include "tag/Chromaprint.hxx"
#include "pcm/PcmConvert.hxx"
#include "event/Thread.hxx"
#include "decoder/DecoderList.hxx"
#include "decoder/DecoderPlugin.hxx"
#include "decoder/DecoderAPI.hxx" /* for class StopDecoder */
#include "input/Init.hxx"
#include "input/InputStream.hxx"
#include "fs/Path.hxx"
#include "AudioFormat.hxx"
#include "util/OptionDef.hxx"
#include "util/OptionParser.hxx"
#include "util/PrintException.hxx"
#include "Log.hxx"
#include "LogBackend.hxx"

#include <stdexcept>

#include <assert.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>

struct CommandLine {
	const char *decoder = nullptr;
	const char *uri = nullptr;

	Path config_path = nullptr;

	bool verbose = false;
};

enum Option {
	OPTION_CONFIG,
	OPTION_VERBOSE,
};

static constexpr OptionDef option_defs[] = {
	{"config", 0, true, "Load a MPD configuration file"},
	{"verbose", 'v', false, "Verbose logging"},
};

static CommandLine
ParseCommandLine(int argc, char **argv)
{
	CommandLine c;

	OptionParser option_parser(option_defs, argc, argv);
	while (auto o = option_parser.Next()) {
		switch (Option(o.index)) {
		case OPTION_CONFIG:
			c.config_path = Path::FromFS(o.value);
			break;

		case OPTION_VERBOSE:
			c.verbose = true;
			break;
		}
	}

	auto args = option_parser.GetRemaining();
	if (args.size != 2)
		throw std::runtime_error("Usage: RunChromaprint [--verbose] [--config=FILE] DECODER URI");

	c.decoder = args[0];
	c.uri = args[1];
	return c;
}

class GlobalInit {
	const ConfigData config;
	EventThread io_thread;
	const ScopeInputPluginsInit input_plugins_init;
	const ScopeDecoderPluginsInit decoder_plugins_init;

public:
	explicit GlobalInit(Path config_path)
		:config(AutoLoadConfigFile(config_path)),
		 input_plugins_init(config, io_thread.GetEventLoop()),
		 decoder_plugins_init(config)
	{
		io_thread.Start();

		pcm_convert_global_init(config);
	}
};

class ChromaprintDecoderClient final : public DecoderClient {
	bool ready = false;

	bool need_convert = false;

	PcmConvert convert;

	Chromaprint::Context chromaprint;

	uint64_t remaining_bytes;

public:
	Mutex mutex;

	~ChromaprintDecoderClient() noexcept {
		if (need_convert)
			convert.Close();
	}

	void PrintResult();

	/* virtual methods from DecoderClient */
	void Ready(AudioFormat audio_format,
		   bool seekable, SignedSongTime duration) override;

	DecoderCommand GetCommand() noexcept override {
		return remaining_bytes > 0
			? DecoderCommand::NONE
			: DecoderCommand::STOP;
	}

	void CommandFinished() override {}

	SongTime GetSeekTime() noexcept override {
		return SongTime::zero();
	}

	uint64_t GetSeekFrame() noexcept override {
		return 0;
	}

	void SeekError() override {}

	InputStreamPtr OpenUri(const char *) override {
		throw std::runtime_error("Not implemented");
	}

	size_t Read(InputStream &is, void *buffer, size_t length) override {
		return is.LockRead(buffer, length);
	}

	void SubmitTimestamp(FloatDuration) override {}
	DecoderCommand SubmitData(InputStream *is,
				  const void *data, size_t length,
				  uint16_t kbit_rate) override;

	DecoderCommand SubmitTag(InputStream *, Tag &&) override {
		return GetCommand();
	}

	void SubmitReplayGain(const ReplayGainInfo *) override {}
	void SubmitMixRamp(MixRampInfo &&) override {}
};

void
ChromaprintDecoderClient::PrintResult()
{
	if (!ready)
		throw std::runtime_error("Decoding failed");

	if (need_convert) {
		auto flushed = convert.Flush();
		auto data = ConstBuffer<int16_t>::FromVoid(flushed);
		chromaprint.Feed(data.data, data.size);
	}

	chromaprint.Finish();

	printf("%s\n", chromaprint.GetFingerprint().c_str());
}

void
ChromaprintDecoderClient::Ready(AudioFormat audio_format, bool, SignedSongTime)
{
	/* feed the first two minutes into libchromaprint */
	remaining_bytes = audio_format.TimeToSize(std::chrono::minutes(2));

	if (audio_format.format != SampleFormat::S16) {
		const AudioFormat src_audio_format = audio_format;
		audio_format.format = SampleFormat::S16;

		convert.Open(src_audio_format, audio_format);
		need_convert = true;
	}

	chromaprint.Start(audio_format.sample_rate, audio_format.channels);

	ready = true;
}

DecoderCommand
ChromaprintDecoderClient::SubmitData(InputStream *,
				     const void *_data, size_t length,
				     uint16_t)
{
	if (length > remaining_bytes)
		remaining_bytes = 0;
	else
		remaining_bytes -= length;

	ConstBuffer<void> src{_data, length};
	ConstBuffer<int16_t> data;

	if (need_convert) {
		auto result = convert.Convert(src);
		data = ConstBuffer<int16_t>::FromVoid(result);
	} else
		data = ConstBuffer<int16_t>::FromVoid(src);

	chromaprint.Feed(data.data, data.size);

	return GetCommand();
}

int main(int argc, char **argv)
try {
	const auto c = ParseCommandLine(argc, argv);

	SetLogThreshold(c.verbose ? LogLevel::DEBUG : LogLevel::INFO);
	const GlobalInit init(c.config_path);

	const DecoderPlugin *plugin = decoder_plugin_from_name(c.decoder);
	if (plugin == nullptr) {
		fprintf(stderr, "No such decoder: %s\n", c.decoder);
		return EXIT_FAILURE;
	}

	ChromaprintDecoderClient client;
	if (plugin->file_decode != nullptr) {
		try {
			plugin->FileDecode(client, Path::FromFS(c.uri));
		} catch (StopDecoder) {
		}
	} else if (plugin->stream_decode != nullptr) {
		auto is = InputStream::OpenReady(c.uri, client.mutex);
		try {
			plugin->StreamDecode(client, *is);
		} catch (StopDecoder) {
		}
	} else {
		fprintf(stderr, "Decoder plugin is not usable\n");
		return EXIT_FAILURE;
	}

	client.PrintResult();
	return EXIT_SUCCESS;
} catch (...) {
	PrintException(std::current_exception());
	return EXIT_FAILURE;
}
