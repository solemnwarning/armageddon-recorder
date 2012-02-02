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

#include "audio.hpp"
#include "main.hpp"

#define FRAME_PREFIX "arec_"
#define SYNC_FRAMES 25
#define ALLOWED_AUDIO_SKEW 5

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
	
	unsigned int orig_detail_level;
	
	audio_recorder *audio_rec;
	HANDLE audio_event;
	
	wav_writer *wav_out;
	
	size_t recorded_frames;
	std::list<WAVEHDR> audio_buffers;
	
	/* Cached result of count_frames() */
	size_t last_frame_count;
	
	wa_capture(const std::string &replay, const arec_config &conf, const std::string &start, const std::string &end);
	~wa_capture();
	
	void worker_main();
	
	bool frame_exists(unsigned int frame);
	unsigned int count_frames();
	
	void flush_audio();
};

#endif /* !AREC_CAPTURE_HPP */
