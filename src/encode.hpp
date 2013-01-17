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

#ifndef AREC_ENCODE_H
#define AREC_ENCODE_H

#include <string>
#include <vector>

#include "main.hpp"

struct ffmpeg_format
{
	const char *name;
	
	const char *codec;
	const char *extra;
};

extern const ffmpeg_format video_formats[];
extern const ffmpeg_format audio_formats[];

struct container_format
{
	const char *ext;
	const char *name;
	
	const char **video_formats;
	const char **audio_formats;
};

extern const container_format container_formats[];

int get_ffmpeg_index(const ffmpeg_format formats[], const std::string &name);

std::vector<int> get_valid_containers(int video_format, int audio_format);

std::string ffmpeg_cmdline();

bool ffmpeg_run();
void ffmpeg_cleanup();

#endif /* !AREC_ENCODE_H */
