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

#ifndef PCM_EXPORT_HXX
#define PCM_EXPORT_HXX

#include "SampleFormat.hxx"
#include "PcmBuffer.hxx"
#include "config.h"

template<typename T> struct ConstBuffer;
struct AudioFormat;

/**
 * An object that handles export of PCM samples to some instance
 * outside of MPD.  It has a few more options to tweak the binary
 * representation which are not supported by the pcm_convert library.
 */
class PcmExport {
	/**
	 * This buffer is used to reorder channels.
	 *
	 * @see #alsa_channel_order
	 */
	PcmBuffer order_buffer;

#ifdef ENABLE_DSD
	/**
	 * The buffer is used to convert DSD samples to the
	 * DoP format.
	 *
	 * @see #dop
	 */
	PcmBuffer dop_buffer;
#endif

	/**
	 * The buffer is used to pack samples, removing padding.
	 *
	 * @see #pack24
	 */
	PcmBuffer pack_buffer;

	/**
	 * The buffer is used to reverse the byte order.
	 *
	 * @see #reverse_endian
	 */
	PcmBuffer reverse_buffer;

	/**
	 * The number of channels.
	 */
	uint8_t channels;

	/**
	 * Convert the given buffer from FLAC channel order to ALSA
	 * channel order using ToAlsaChannelOrder()?
	 *
	 * If this value is SampleFormat::UNDEFINED, then no channel
	 * reordering is applied, otherwise this is the input sample
	 * format.
	 */
	SampleFormat alsa_channel_order;

#ifdef ENABLE_DSD
	/**
	 * Convert DSD (U8) to DSD_U16?
	 */
	bool dsd_u16;

	/**
	 * Convert DSD (U8) to DSD_U32?
	 */
	bool dsd_u32;

	/**
	 * Convert DSD to DSD-over-PCM (DoP)?  Input format must be
	 * SampleFormat::DSD and output format must be
	 * SampleFormat::S24_P32.
	 */
	bool dop;
#endif

	/**
	 * Convert (padded) 24 bit samples to 32 bit by shifting 8
	 * bits to the left?
	 */
	bool shift8;

	/**
	 * Pack 24 bit samples?
	 */
	bool pack24;

	/**
	 * Export the samples in reverse byte order?  A non-zero value
	 * means the option is enabled and represents the size of each
	 * sample (2 or bigger).
	 */
	uint8_t reverse_endian;

public:
	struct Params {
		bool alsa_channel_order = false;
#ifdef ENABLE_DSD
		bool dsd_u16 = false;
		bool dsd_u32 = false;
		bool dop = false;
#endif
		bool shift8 = false;
		bool pack24 = false;
		bool reverse_endian = false;

		/**
		 * Calculate the output sample rate, given a specific input
		 * sample rate.  Usually, both are the same; however, with
		 * DSD_U32, four input bytes (= 4 * 8 bits) are combined to
		 * one output word (32 bits), dividing the sample rate by 4.
		 */
		gcc_pure
		unsigned CalcOutputSampleRate(unsigned input_sample_rate) const noexcept;

		/**
		 * The inverse of CalcOutputSampleRate().
		 */
		gcc_pure
		unsigned CalcInputSampleRate(unsigned output_sample_rate) const noexcept;
	};

	/**
	 * Open the object.
	 *
	 * There is no "close" method.  This function may be called multiple
	 * times to reuse the object.
	 *
	 * This function cannot fail.
	 *
	 * @param channels the number of channels; ignored unless dop is set
	 */
	void Open(SampleFormat sample_format, unsigned channels,
		  Params params) noexcept;

	/**
	 * Reset the filter's state, e.g. drop/flush buffers.
	 */
	void Reset() noexcept {
	}

	/**
	 * Calculate the size of one output frame.
	 */
	gcc_pure
	size_t GetFrameSize(const AudioFormat &audio_format) const noexcept;

	/**
	 * Export a PCM buffer.
	 *
	 * @param src the source PCM buffer
	 * @return the destination buffer; may be empty (and may be a
	 * pointer to the source buffer)
	 */
	ConstBuffer<void> Export(ConstBuffer<void> src) noexcept;

	/**
	 * Converts the number of consumed bytes from the pcm_export()
	 * destination buffer to the according number of bytes from the
	 * pcm_export() source buffer.
	 */
	gcc_pure
	size_t CalcSourceSize(size_t dest_size) const noexcept;
};

#endif
