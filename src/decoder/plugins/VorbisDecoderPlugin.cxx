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
#include "VorbisDecoderPlugin.h"
#include "OggDecoder.hxx"
#include "lib/xiph/VorbisComments.hxx"
#include "lib/xiph/OggPacket.hxx"
#include "lib/xiph/OggFind.hxx"
#include "VorbisDomain.hxx"
#include "../DecoderAPI.hxx"
#include "input/InputStream.hxx"
#include "input/Reader.hxx"
#include "OggCodec.hxx"
#include "pcm/Interleave.hxx"
#include "util/Macros.hxx"
#include "util/ScopeExit.hxx"
#include "CheckAudioFormat.hxx"
#include "tag/TagHandler.hxx"
#include "Log.hxx"

#ifndef HAVE_TREMOR
#include <vorbis/codec.h>
#else
#include <tremor/ivorbiscodec.h>
#endif /* HAVE_TREMOR */

#include <stdexcept>

class VorbisDecoder final : public OggDecoder {
#ifdef HAVE_TREMOR
	static constexpr SampleFormat sample_format = SampleFormat::S16;
	typedef ogg_int32_t in_sample_t;
	typedef int16_t out_sample_t;
#else
	static constexpr SampleFormat sample_format = SampleFormat::FLOAT;
	typedef float in_sample_t;
	typedef float out_sample_t;
#endif

	unsigned remaining_header_packets;

	vorbis_info vi;
	vorbis_comment vc;
	vorbis_dsp_state dsp;
	vorbis_block block;

	/**
	 * If non-zero, then a previous Vorbis stream has been found
	 * already with this number of channels.
	 */
	AudioFormat audio_format = AudioFormat::Undefined();
	size_t frame_size;

	bool dsp_initialized = false;

public:
	explicit VorbisDecoder(DecoderReader &reader)
		:OggDecoder(reader) {
		InitVorbis();
	}

	~VorbisDecoder() {
		DeinitVorbis();
	}

	bool Seek(uint64_t where_frame);

private:
	void InitVorbis() {
		vorbis_info_init(&vi);
		vorbis_comment_init(&vc);
	}

	void DeinitVorbis() {
		if (dsp_initialized) {
			dsp_initialized = false;

			vorbis_block_clear(&block);
			vorbis_dsp_clear(&dsp);
		}

		vorbis_comment_clear(&vc);
		vorbis_info_clear(&vi);
	}

	void ReinitVorbis() {
		DeinitVorbis();
		InitVorbis();
	}

	void SubmitInit();
	bool SubmitSomePcm();
	void SubmitPcm();

protected:
	/* virtual methods from class OggVisitor */
	void OnOggBeginning(const ogg_packet &packet) override;
	void OnOggPacket(const ogg_packet &packet) override;
	void OnOggEnd() override;
};

bool
VorbisDecoder::Seek(uint64_t where_frame)
{
	assert(IsSeekable());
	assert(input_stream.IsSeekable());
	assert(input_stream.KnownSize());

	const ogg_int64_t where_granulepos(where_frame);

	try {
		SeekGranulePos(where_granulepos);
		vorbis_synthesis_restart(&dsp);
		return true;
	} catch (const std::runtime_error &) {
		return false;
	}
}

void
VorbisDecoder::OnOggBeginning(const ogg_packet &_packet)
{
	/* libvorbis wants non-const packets */
	ogg_packet &packet = const_cast<ogg_packet &>(_packet);

	ReinitVorbis();

	if (vorbis_synthesis_headerin(&vi, &vc, &packet) != 0)
		throw std::runtime_error("Unrecognized Vorbis BOS packet");

	remaining_header_packets = 2;
}

static void
vorbis_send_comments(DecoderClient &client, InputStream &is,
		     char **comments)
{
	Tag *tag = vorbis_comments_to_tag(comments);
	if (!tag)
		return;

	client.SubmitTag(is, std::move(*tag));
	delete tag;
}

void
VorbisDecoder::SubmitInit()
{
	assert(!dsp_initialized);

	audio_format = CheckAudioFormat(vi.rate, sample_format, vi.channels);

	frame_size = audio_format.GetFrameSize();

	const auto eos_granulepos = UpdateEndGranulePos();
	const auto duration = eos_granulepos >= 0
		? SignedSongTime::FromScale<uint64_t>(eos_granulepos,
						      audio_format.sample_rate)
		: SignedSongTime::Negative();

	client.Ready(audio_format, eos_granulepos > 0, duration);
}

bool
VorbisDecoder::SubmitSomePcm()
{
	in_sample_t **pcm;
	int result = vorbis_synthesis_pcmout(&dsp, &pcm);
	if (result <= 0)
		return false;

	out_sample_t buffer[4096];
	const unsigned channels = audio_format.channels;
	size_t max_frames = ARRAY_SIZE(buffer) / channels;
	size_t n_frames = std::min(size_t(result), max_frames);

#ifdef HAVE_TREMOR
	for (unsigned c = 0; c < channels; ++c) {
		const auto *src = pcm[c];
		auto *dest = &buffer[c];

		for (size_t i = 0; i < n_frames; ++i) {
			*dest = *src++;
			dest += channels;
		}
	}
#else
	PcmInterleaveFloat(buffer,
			   ConstBuffer<const in_sample_t *>(pcm,
							    channels),
			   n_frames);
#endif

	vorbis_synthesis_read(&dsp, n_frames);

	const size_t nbytes = n_frames * frame_size;
	auto cmd = client.SubmitData(input_stream,
				     buffer, nbytes,
				     0);
	if (cmd != DecoderCommand::NONE)
		throw cmd;

	return true;
}

void
VorbisDecoder::SubmitPcm()
{
	while (SubmitSomePcm()) {}
}

void
VorbisDecoder::OnOggPacket(const ogg_packet &_packet)
{
	/* libvorbis wants non-const packets */
	ogg_packet &packet = const_cast<ogg_packet &>(_packet);

	if (remaining_header_packets > 0) {
		if (vorbis_synthesis_headerin(&vi, &vc, &packet) != 0)
			throw std::runtime_error("Unrecognized Vorbis header packet");

		if (--remaining_header_packets > 0)
			return;

		if (audio_format.IsDefined()) {
			/* TODO: change the MPD decoder plugin API to
			   allow mid-song AudioFormat changes */
			if ((unsigned)vi.rate != audio_format.sample_rate ||
			    (unsigned)vi.channels != audio_format.channels)
				throw std::runtime_error("Next stream has different audio format");
		} else
			SubmitInit();

		vorbis_send_comments(client, input_stream, vc.user_comments);

		ReplayGainInfo rgi;
		if (vorbis_comments_to_replay_gain(rgi, vc.user_comments))
			client.SubmitReplayGain(&rgi);
	} else {
		if (!dsp_initialized) {
			dsp_initialized = true;

			vorbis_synthesis_init(&dsp, &vi);
			vorbis_block_init(&dsp, &block);
		}

		if (vorbis_synthesis(&block, &packet) != 0) {
			/* ignore bad packets, but give the MPD core a
			   chance to stop us */
			auto cmd = client.GetCommand();
			if (cmd != DecoderCommand::NONE)
				throw cmd;
			return;
		}

		if (vorbis_synthesis_blockin(&dsp, &block) != 0)
			throw std::runtime_error("vorbis_synthesis_blockin() failed");

		SubmitPcm();

#ifndef HAVE_TREMOR
		if (packet.granulepos > 0)
			client.SubmitTimestamp(vorbis_granule_time(&dsp, packet.granulepos));
#endif
	}
}

void
VorbisDecoder::OnOggEnd()
{
}

/* public */

static bool
vorbis_init(gcc_unused const ConfigBlock &block)
{
#ifndef HAVE_TREMOR
	LogDebug(vorbis_domain, vorbis_version_string());
#endif
	return true;
}

static void
vorbis_stream_decode(DecoderClient &client,
		     InputStream &input_stream)
{
	if (ogg_codec_detect(&client, input_stream) != OGG_CODEC_VORBIS)
		return;

	/* rewind the stream, because ogg_codec_detect() has
	   moved it */
	try {
		input_stream.LockRewind();
	} catch (const std::runtime_error &) {
	}

	DecoderReader reader(client, input_stream);
	VorbisDecoder d(reader);

	while (true) {
		try {
			d.Visit();
			break;
		} catch (DecoderCommand cmd) {
			if (cmd == DecoderCommand::SEEK) {
				if (d.Seek(client.GetSeekFrame()))
					client.CommandFinished();
				else
					client.SeekError();
			} else if (cmd != DecoderCommand::NONE)
				break;
		}
	}
}

static void
VisitVorbisDuration(InputStream &is,
		    OggSyncState &sync, OggStreamState &stream,
		    unsigned sample_rate,
		    const TagHandler &handler, void *handler_ctx)
{
	ogg_packet packet;

	if (!OggSeekFindEOS(sync, stream, packet, is))
		return;

	const auto duration =
		SongTime::FromScale<uint64_t>(packet.granulepos,
					      sample_rate);
	tag_handler_invoke_duration(handler, handler_ctx, duration);
}

static bool
vorbis_scan_stream(InputStream &is,
		   const TagHandler &handler, void *handler_ctx)
{
	/* initialize libogg */

	InputStreamReader reader(is);
	OggSyncState sync(reader);

	ogg_page first_page;
	if (!sync.ExpectPage(first_page))
		return false;

	OggStreamState stream(first_page);

	/* initialize libvorbis */

	vorbis_info vi;
	vorbis_info_init(&vi);
	AtScopeExit(&) { vorbis_info_clear(&vi); };

	vorbis_comment vc;
	vorbis_comment_init(&vc);
	AtScopeExit(&) { vorbis_comment_clear(&vc); };

	/* feed the first 3 packets to libvorbis */

	for (unsigned i = 0; i < 3; ++i) {
		ogg_packet packet;
		if (!OggReadPacket(sync, stream, packet) ||
		    vorbis_synthesis_headerin(&vi, &vc, &packet) != 0)
			return false;
	}

	/* visit the Vorbis comments we just read */

	vorbis_comments_scan(vc.user_comments,
			     handler, handler_ctx);

	/* check the song duration by locating the e_o_s packet */

	VisitVorbisDuration(is, sync, stream, vi.rate, handler, handler_ctx);

	return true;
}

static const char *const vorbis_suffixes[] = {
	"ogg", "oga", nullptr
};

static const char *const vorbis_mime_types[] = {
	"application/ogg",
	"application/x-ogg",
	"audio/ogg",
	"audio/vorbis",
	"audio/vorbis+ogg",
	"audio/x-ogg",
	"audio/x-vorbis",
	"audio/x-vorbis+ogg",
	nullptr
};

const struct DecoderPlugin vorbis_decoder_plugin = {
	"vorbis",
	vorbis_init,
	nullptr,
	vorbis_stream_decode,
	nullptr,
	nullptr,
	vorbis_scan_stream,
	nullptr,
	vorbis_suffixes,
	vorbis_mime_types
};
