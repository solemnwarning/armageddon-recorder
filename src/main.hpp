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
#include <stdexcept>

#include "reg.hpp"

#define LIST_HEIGHT 200

extern std::string wa_path;
extern bool wormkit_exe;

extern reg_handle wa_options;

template<class T> std::string to_string(const T& in) {
	std::stringstream os;
	os << in;
	
	return os.str();
};

struct arec_config {
	/* Last browsed directories in open/save dialogs */
	std::string replay_dir;
	std::string video_dir;
	
	std::string replay_file, replay_name;	/* Full path and final component of replay filename */
	std::string capture_dir;		/* Directory containing frames/audio */
	
	unsigned int width, height;
	unsigned int frame_rate;
	
	std::string start_time, end_time;
	
	/* Output video file name/formats */
	std::string video_file;
	unsigned int video_format;
	unsigned int audio_format;
	
	unsigned int max_enc_threads;
	
	unsigned int wa_detail_level;
	unsigned int wa_chat_behaviour;
	bool wa_lock_camera;
	bool wa_bigger_fonts;
	bool wa_transparent_labels;
	
	bool load_wormkit_dlls;
	
	bool do_cleanup;
	
	/* Audio settings */
	
	int init_vol;
	
	bool fix_clipping;
	int step_vol, min_vol;
};

extern arec_config config;

const char *w32_error(DWORD errnum);
std::string escape_filename(std::string name);

#endif /* !AREC_MAIN_HPP */
