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
#include <string>
#include <vector>
#include <map>
#include <stdint.h>
#include <gorilla/ga.h>

#include "main.hpp"

/* Format to use when generating the game audio. */
#define SAMPLE_RATE 44100
#define SAMPLE_BITS 16
#define CHANNELS    2

struct wav_file
{
	std::string path;
	
	ga_Sound *sound;
	
	wav_file(const std::string &_path);
	wav_file(const wav_file &src);
	
	~wav_file();
};

extern std::map<uint32_t, wav_file> wav_files;

void init_wav_search_path();

uint32_t get_wav_file_hash(const char *path);
wav_file *wav_search(uint32_t hash, std::string path);
wav_file *get_wav_file(uint32_t hash);

bool make_output_wav();

#endif /* !AREC_AUDIO_HPP */
