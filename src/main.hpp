/* Armageddon Recorder
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

#ifndef AREC_MAIN_HPP
#define AREC_MAIN_HPP

#include <string>
#include <sstream>

#include "reg.hpp"

#define LIST_HEIGHT 200

extern bool wormkit_present;
extern reg_handle wa_options;

template<class T> std::string to_string(const T& in) {
	std::stringstream os;
	os << in;
	
	return os.str();
};

struct arec_config {
	std::string replay_dir;
	std::string video_dir;
	
	unsigned int width, height;
	unsigned int frame_rate;
	
	std::string start_time, end_time;
	
	bool enable_audio;
	unsigned int audio_source;
	bool do_second_pass;
	
	unsigned int audio_rate;
	unsigned int audio_channels;
	unsigned int audio_bits;
	
	unsigned int audio_buf_time;
	unsigned int audio_buf_count;
	unsigned int max_skew;
	
	unsigned int max_enc_threads;
	
	unsigned int wa_detail_level;
	unsigned int wa_chat_behaviour;
	bool wa_lock_camera;
	bool wa_bigger_fonts;
	bool wa_transparent_labels;
	
	bool load_wormkit_dlls;
};

#endif /* !AREC_MAIN_HPP */
