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

#include "ConvertFilterPlugin.hxx"
#include "filter/Filter.hxx"
#include "filter/Prepared.hxx"
#include "pcm/PcmConvert.hxx"
#include "util/Manual.hxx"
#include "util/ConstBuffer.hxx"
#include "AudioFormat.hxx"

#include <stdexcept>
#include <memory>

#include <assert.h>

class ConvertFilter final : public Filter {
	/**
	 * The input audio format; PCM data is passed to the filter()
	 * method in this format.
	 */
	AudioFormat in_audio_format;

	/**
	 * This object is only "open" if #in_audio_format !=
	 * #out_audio_format.
	 */
	PcmConvert state;

public:
	ConvertFilter(const AudioFormat &audio_format);
	~ConvertFilter();

	void Set(const AudioFormat &_out_audio_format);

	void Reset() noexcept override {
		if (IsActive())
			state.Reset();
	}

	ConstBuffer<void> FilterPCM(ConstBuffer<void> src) override;

	ConstBuffer<void> Flush() override {
		return IsActive()
			? state.Flush()
			: nullptr;
	}

private:
	bool IsActive() const noexcept {
		return out_audio_format != in_audio_format;
	}
};

class PreparedConvertFilter final : public PreparedFilter {
public:
	std::unique_ptr<Filter> Open(AudioFormat &af) override;
};

void
ConvertFilter::Set(const AudioFormat &_out_audio_format)
{
	assert(in_audio_format.IsValid());
	assert(_out_audio_format.IsValid());

	if (_out_audio_format == out_audio_format)
		/* no change */
		return;

	if (IsActive()) {
		out_audio_format = in_audio_format;
		state.Close();
	}

	if (_out_audio_format == in_audio_format)
		/* optimized special case: no-op */
		return;

	state.Open(in_audio_format, _out_audio_format);

	out_audio_format = _out_audio_format;
}

ConvertFilter::ConvertFilter(const AudioFormat &audio_format)
	:Filter(audio_format), in_audio_format(audio_format)
{
}

std::unique_ptr<Filter>
PreparedConvertFilter::Open(AudioFormat &audio_format)
{
	assert(audio_format.IsValid());

	return std::make_unique<ConvertFilter>(audio_format);
}

ConvertFilter::~ConvertFilter()
{
	assert(in_audio_format.IsValid());

	if (IsActive())
		state.Close();
}

ConstBuffer<void>
ConvertFilter::FilterPCM(ConstBuffer<void> src)
{
	assert(in_audio_format.IsValid());

	return IsActive()
		? state.Convert(src)
		/* optimized special case: no-op */
		: src;
}

std::unique_ptr<PreparedFilter>
convert_filter_prepare() noexcept
{
	return std::make_unique<PreparedConvertFilter>();
}

Filter *
convert_filter_new(const AudioFormat in_audio_format,
		   const AudioFormat out_audio_format)
{
	std::unique_ptr<ConvertFilter> filter(new ConvertFilter(in_audio_format));
	filter->Set(out_audio_format);
	return filter.release();
}

void
convert_filter_set(Filter *_filter, AudioFormat out_audio_format)
{
	ConvertFilter *filter = (ConvertFilter *)_filter;

	filter->Set(out_audio_format);
}
