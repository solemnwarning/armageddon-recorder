/* Armageddon Recorder - Encoder stuff
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
#include <vfw.h>

#include "encode.hpp"

std::vector<encoder_info> encoders;

std::string fcc_to_string(DWORD fcc) {
	char buf[8];
	memcpy(buf, &fcc, 4);
	buf[5] = '\0';
	
	return buf;
}

std::string wide_to_string(const WCHAR *wide) {
	std::string buf;
	
	for(unsigned int i = 0; wide[i]; i++) {
		buf.push_back(wide[i]);
	}
	
	return buf;
}

#define ADD_ENCODER(...) encoders.push_back(encoder_info(__VA_ARGS__))

void load_encoders() {
	ADD_ENCODER("Don't create video", encoder_info::none, 0, NULL, NULL, NULL); // frames
	
	ADD_ENCODER("H.264 (Low quality)", encoder_info::ffmpeg, 0.325, "libx264 -pix_fmt yuvj420p", "libvo_aacenc", "mp4");
	ADD_ENCODER("H.264 (Medium quality)", encoder_info::ffmpeg, 0.98, "libx264 -pix_fmt yuvj420p", "libvo_aacenc", "mp4");
	ADD_ENCODER("H.264 (High quality)", encoder_info::ffmpeg, 3.6, "libx264 -pix_fmt yuvj444p", "libvo_aacenc", "mp4");
	ADD_ENCODER("H.264 (Lossless)", encoder_info::ffmpeg, 0, "libx264rgb", "libvo_aacenc", "mp4");
	
	ADD_ENCODER("Uncompressed AVI", encoder_info::ffmpeg, 0, "rawvideo -pix_fmt bgr24", "pcm_s16le", "avi");
}
