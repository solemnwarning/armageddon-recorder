/* Armageddon Recorder - Capture code
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

#ifndef AREC_CAPTURE_HPP
#define AREC_CAPTURE_HPP

#include <windows.h>
#include <string>
#include <list>
#include <map>
#include <vector>
#include <time.h>

#include "audio.hpp"
#include "main.hpp"

#define FRAME_PREFIX "arec_"

#define MAX_WA_LOAD_TIME 10
#define WA_INPUT_FRAMES 5

#define WM_WAEXIT WM_USER+1
#define WM_PUSHLOG WM_USER+2

struct audio_buf {
	char *data;
	size_t size;	/* Amount of audio data in buffer */
	
	size_t frames;	/* Number of frames present when buffer was popped */
	
	audio_buf(char *d, size_t s, size_t f): data(d), size(s), frames(f) {}
};

struct wa_capture {
	std::string replay_path;
	std::string capture_path;
	
	arec_config config;
	
	HANDLE worker_thread;
	HANDLE force_exit;
	
	char *worms_cmdline;
	HANDLE worms_process;
	time_t worms_started;
	
	HMODULE madchook;
	
	std::map<std::string,DWORD> original_options;
	
	audio_recorder *audio_rec;
	HANDLE audio_event;
	
	wav_writer *wav_out;
	
	size_t recorded_frames;
	std::list<WAVEHDR> audio_buffers;
	
	int this_pass;
	
	size_t p1_skew_bytes;	/* How many unexpected bytes have been written to
				 * the first pass audio stream. This counter is reset
				 * every time the audio is chopped or padded.
				*/
	
	HANDLE p2_s1_timer;	/* Timer to press/release the 1 key while capturing second pass */
	bool p2_s1_held;	/* Current state of the 1 key */
	
	/* Cached result of count_frames() */
	size_t last_frame_count;
	
	/* Variables used for 2-pass synchronization */
	
	int dead_min, dead_max;
	
	wa_capture(const std::string &replay, const arec_config &conf);
	~wa_capture();
	
	void worker_main();
	
	bool frame_exists(unsigned int frame);
	unsigned int count_frames();
	
	void flush_audio();
	void pass2_flush();
	
	void set_option(const char *name, DWORD value, DWORD def_value = 0);
	
	void start_wa(std::string cmdline);
	
	std::vector<int16_t> gen_averages(char *raw_pcm, size_t samples, int16_t dead_val);
	unsigned int calc_variation(const std::vector<int16_t> &a, size_t a_min, const std::vector<int16_t> &b, size_t b_min);
	void pad_deadzone(const char *raw_pcm, size_t samples);
};

#endif /* !AREC_CAPTURE_HPP */
