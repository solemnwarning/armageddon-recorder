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
#include <sndfile.h>

#include "main.hpp"

/* Format to use when generating the game audio. */
#define SAMPLE_RATE 44100
#define SAMPLE_BITS 16
#define CHANNELS    2

bool make_output_wav();

extern "C"
{
	unsigned int pcm_resample_frames(unsigned int frames, int rate_in, int rate_out);
	unsigned int pcm_resample_bufsize(unsigned int frames, int channels, int rate_in, int rate_out, int bits_out);
	void pcm_resample(const void *pcm_in, const void *pcm_out, unsigned int frames, int channels, int rate_in, int rate_out, int bits_in, int bits_out);
}

#endif /* !AREC_AUDIO_HPP */
