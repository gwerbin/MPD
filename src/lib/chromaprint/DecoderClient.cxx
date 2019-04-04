/*
 * Copyright 2003-2019 The Music Player Daemon Project
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

#include "DecoderClient.hxx"
#include "input/InputStream.hxx"
#include "util/ConstBuffer.hxx"

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

size_t
ChromaprintDecoderClient::Read(InputStream &is, void *buffer, size_t length)
{
	return is.LockRead(buffer, length);
}
