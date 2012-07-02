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

/* Win32 API calls that are unlikely to fail (i.e CreateEvent) */
#define BASIC_W32_ASSERT(expr) { \
	if(!(expr)) { \
		throw arec::error(std::string("Win32 function call failed at ") + __FILE__ + ":" + to_string(__LINE__) + ":\r\n" + #expr + "\r\nGetLastError() = " + to_string(GetLastError()) + " (" + w32_error(GetLastError()) + ")"); \
	} \
}

/* Asserts that should never ever ever fail.
 * Use only to verify internal consistencies.
*/
#define INTERNAL_ASSERT(expr) { \
	if(!(expr)) { \
		throw arec::error(std::string("Internal error at ") + __FILE__ + ":" + to_string(__LINE__) + ":\r\nAssertion: " + #expr); \
	} \
}

extern bool wormkit_exe, wormkit_ds;
extern reg_handle wa_options;

namespace arec {
	struct fatal_error: public std::runtime_error {
		fatal_error(const std::string &err): runtime_error(err) {}
	};
	
	struct error: public std::runtime_error {
		error(const std::string &err): runtime_error(err) {}
	};
};

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
	
	bool enable_audio;
	unsigned int audio_source;
	bool do_second_pass;
	
	unsigned int audio_rate;
	unsigned int audio_channels;
	unsigned int audio_bits;
	
	unsigned int audio_buf_time;
	unsigned int audio_buf_count;
	unsigned int max_skew;
	
	unsigned int sp_buffer;
	unsigned int sp_mean_frames;
	unsigned int sp_cmp_frames;
	
	bool sp_use_dz, sp_dynamic_dz;
	double sp_static_dz, sp_dz_margin;
	
	unsigned int max_enc_threads;
	
	unsigned int wa_detail_level;
	unsigned int wa_chat_behaviour;
	bool wa_lock_camera;
	bool wa_bigger_fonts;
	bool wa_transparent_labels;
	
	bool load_wormkit_dlls;
	
	bool do_cleanup;
};

extern arec_config config;

const char *w32_error(DWORD errnum);
std::string escape_filename(std::string name);

#endif /* !AREC_MAIN_HPP */
