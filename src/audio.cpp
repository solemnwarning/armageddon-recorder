/* Armageddon Recorder - Audio functions
 * Copyright (C) 2012 Daniel Collins <solemnwarning@solemnwarning.net>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <windows.h>
#include <string>
#include <stdio.h>
#include <assert.h>

#include "audio.hpp"

audio_recorder::audio_recorder(const arec_config &config, HANDLE ev): event(ev) {
	WAVEFORMATEX format;
	format.wFormatTag = WAVE_FORMAT_PCM;
	format.nChannels = CHANNELS;
	format.nSamplesPerSec = SAMPLE_RATE;
	format.nAvgBytesPerSec = BYTES_SEC;
	format.nBlockAlign = BLOCK_ALIGN;
	format.wBitsPerSample = SAMPLE_BITS;
	format.cbSize = 0;
	
	buf_size = ((CHANNELS * (SAMPLE_BITS / 8) * SAMPLE_RATE) / config.frame_rate) * config.audio_buf_time;
	
	MMRESULT err = waveInOpen(
		&wavein,
		config.audio_source,
		&format,
		(DWORD_PTR)event,
		(DWORD_PTR)GetModuleHandle(NULL),
		CALLBACK_EVENT
	);
	
	assert(err == MMSYSERR_NOERROR);
	
	for(unsigned int i = 0; i < config.audio_buf_count; i++) {
		add_buffer();
	}
}

audio_recorder::~audio_recorder() {
	waveInReset(wavein);
	
	while(!buffers.empty() && buffers.front().dwFlags & WHDR_DONE) {
		assert(waveInUnprepareHeader(wavein, &(buffers.front()), sizeof(WAVEHDR)) == MMSYSERR_NOERROR);
		
		delete buffers.front().lpData;
		buffers.pop_front();
	}
	
	assert(waveInClose(wavein) == MMSYSERR_NOERROR);
}

void audio_recorder::start() {
	assert(waveInStart(wavein) == MMSYSERR_NOERROR);
}

void audio_recorder::stop() {
	assert(waveInStop(wavein) == MMSYSERR_NOERROR);
}

void audio_recorder::add_buffer() {
	WAVEHDR header;
	
	memset(&header, 0, sizeof(header));
	header.lpData = new char[buf_size];
	header.dwBufferLength = buf_size;
	
	buffers.push_back(header);
	
	assert(waveInPrepareHeader(wavein, &(buffers.back()), sizeof(header)) == MMSYSERR_NOERROR);
	assert(waveInAddBuffer(wavein, &(buffers.back()), sizeof(header)) == MMSYSERR_NOERROR);
}

std::list<WAVEHDR> audio_recorder::get_buffers() {
	std::list<WAVEHDR> r_buffers;
	
	while(buffers.front().dwFlags & WHDR_DONE) {
		assert(waveInUnprepareHeader(wavein, &(buffers.front()), sizeof(WAVEHDR)) == MMSYSERR_NOERROR);
		
		r_buffers.push_back(buffers.front());
		buffers.pop_front();
		
		add_buffer();
	}
	
	return r_buffers;
}

std::vector<WAVEINCAPS> get_audio_sources() {
	std::vector<WAVEINCAPS> ret;
	
	unsigned int num_devs = waveInGetNumDevs();
	
	for(unsigned int i = 0; i < num_devs; i++) {
		WAVEINCAPS device;
		
		MMRESULT err = waveInGetDevCaps(i, &device, sizeof(device));
		if(err != MMSYSERR_NOERROR) {
			MessageBox(NULL, std::string("waveInGetDevCaps: " + wave_error(err)).c_str(), NULL, MB_OK | MB_ICONERROR | MB_TASKMODAL);
			break;
		}
		
		ret.push_back(device);
	}
	
	return ret;
}

std::string wave_error(MMRESULT errnum) {
	char err[256];
	waveInGetErrorText(errnum, err, sizeof(err));
	
	return err;
}

wav_writer::wav_writer(const std::string &filename, int channels, int rate, int width) {
	sample_size = channels * (width / 8);
	sample_rate = rate;
	
	header.chunk1_channels = channels;
	header.chunk1_sample_rate = rate;
	header.chunk1_bits_sample = width;
	
	header.chunk1_byte_rate = rate * sample_size;
	header.chunk1_align = sample_size;
	
	assert((file = fopen(filename.c_str(), "wb")));
}

wav_writer::~wav_writer() {
	fclose(file);
}

void wav_writer::force_length(size_t samples) {
	size_t file_samples = header.chunk2_size / header.chunk1_align;
	
	if(samples < file_samples) {
		size_t size = header.chunk1_align * (file_samples - samples);
		
		header.chunk0_size -= size;
		header.chunk2_size -= size;
		
		write_at(0, &header, sizeof(header));
		
		/* TODO: Truncate file length */
	}else if(samples > file_samples) {
		size_t size = header.chunk1_align * (samples - file_samples);
		
		char *buf = new char[size];
		memset(buf, 0, size);
		
		append_data(buf, size);
		
		delete buf;
	}
}

void wav_writer::write_at(size_t offset, const void *data, size_t size) {
	assert(fseek(file, offset, SEEK_SET) == 0);
	
	for(size_t w = 0; w < size;) {
		w += fwrite(((char*)data) + w, 1, size - w, file);
		assert(!ferror(file));
	}
}

void wav_writer::append_data(const void *data, size_t size) {
	assert((size % sample_size) == 0);
	
	write_at(sizeof(header) + header.chunk2_size, data, size);
	
	header.chunk0_size += size;
	header.chunk2_size += size;
	
	write_at(0, &header, sizeof(header));
}
