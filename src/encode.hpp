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

struct encoder_info {
	enum type_enum {
		none = 0,
		ffmpeg
	};
	
	std::string name;
	type_enum type;
	
	double bps_pix;
	const char *default_ext;
	
	const char *video_format;
	
	encoder_info(const std::string &n, type_enum t, double bps, const char *vf, const char *ext): name(n), type(t), bps_pix(bps), default_ext(ext), video_format(vf) {}
};

struct audio_encoder {
	const char *name;
	const char *desc;
};

extern std::vector<encoder_info> encoders;
extern audio_encoder audio_encoders[];

void load_encoders();

std::string ffmpeg_cmdline(const arec_config &config);

void ffmpeg_run(const arec_config &config);
void ffmpeg_cleanup();

#endif /* !AREC_ENCODE_H */
