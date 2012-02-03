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

#ifndef AREC_AUDIO_HPP
#define AREC_AUDIO_HPP

#include <windows.h>
#include <list>
#include <string>
#include <vector>
#include <stdint.h>
#include <stdio.h>

#include "main.hpp"

#define WAVE_FORMAT_96M08 0x00010000
#define WAVE_FORMAT_96S08 0x00020000
#define WAVE_FORMAT_96M16 0x00040000
#define WAVE_FORMAT_96S16 0x00080000

extern std::vector<WAVEINCAPS> audio_sources;

struct wav_hdr {
	uint32_t chunk0_id;
	uint32_t chunk0_size;
	uint32_t chunk0_format;
	
	uint32_t chunk1_id;
	uint32_t chunk1_size;
	uint16_t chunk1_format;
	uint16_t chunk1_channels;
	uint32_t chunk1_sample_rate;
	uint32_t chunk1_byte_rate;
	uint16_t chunk1_align;
	uint16_t chunk1_bits_sample;
	
	uint32_t chunk2_id;
	uint32_t chunk2_size;
	
	wav_hdr() {
		memcpy(&chunk0_id, "RIFF", 4);
		chunk0_size = sizeof(wav_hdr) - 8;
		memcpy(&chunk0_format, "WAVE", 4);
		
		memcpy(&chunk1_id, "fmt ", 4);
		chunk1_size = 16;
		chunk1_format = 1;
		//chunk1_channels = CHANNELS;
		//chunk1_sample_rate = SAMPLE_RATE;
		//chunk1_byte_rate = BYTES_SEC;
		//chunk1_align = BLOCK_ALIGN;
		//chunk1_bits_sample = SAMPLE_BITS;
		
		memcpy(&chunk2_id, "data", 4);
		chunk2_size = 0;
	}
};

struct audio_recorder {
	HANDLE &event;
	
	HWAVEIN wavein;
	std::list<WAVEHDR> buffers;
	
	unsigned int buf_size;
	
	audio_recorder(const arec_config &config, HANDLE ev);
	~audio_recorder();
	
	void start();
	void stop();
	
	void add_buffer();
	std::list<WAVEHDR> get_buffers();
};

struct wav_writer {
	wav_hdr header;
	FILE *file;
	
	unsigned int sample_size;
	unsigned int sample_rate;
	
	wav_writer(const arec_config &config, const std::string &filename);
	~wav_writer();
	
	void force_length(size_t samples);
	
	void write_at(size_t offset, const void *data, size_t size);
	void append_data(const void *data, size_t size);
};

std::vector<WAVEINCAPS> get_audio_sources();

std::string wave_error(MMRESULT errnum);

bool test_audio_format(unsigned int source_id, unsigned int rate, unsigned int channels, unsigned int bits);

#endif /* !AREC_AUDIO_HPP */
