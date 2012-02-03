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

#include "reg.hpp"

#define LIST_HEIGHT 200

extern reg_handle wa_options;

struct arec_config {
	std::string replay_dir;
	std::string video_dir;
	
	unsigned int width, height;
	unsigned int frame_rate;
	
	bool enable_audio;
	unsigned int audio_source;
	
	unsigned int audio_buf_time;
	unsigned int audio_buf_count;
	unsigned int max_skew;
	
	unsigned int max_enc_threads;
	
	unsigned int wa_detail_level;
	unsigned int wa_chat_behaviour;
};

#endif /* !AREC_MAIN_HPP */
