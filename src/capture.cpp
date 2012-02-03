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

#include <windows.h>
#include <sstream>
#include <assert.h>

#include "capture.hpp"
#include "audio.hpp"

void delete_frames(const std::string &dir);
void log_push(const std::string &msg);

extern std::string wa_path;

extern HWND progress_dialog;

template<class T> std::string to_string(const T& in) {
	std::stringstream os;
	os << in;
	
	return os.str();
};

static DWORD WINAPI capture_worker_init(LPVOID this_ptr) {
	wa_capture *t = (wa_capture*)this_ptr;
	t->worker_main();
	
	return 0;
}

wa_capture::wa_capture(const std::string &replay, const arec_config &conf, const std::string &start, const std::string &end): config(conf) {
	std::string replay_name = replay_path = replay;
	
	size_t last_slash = replay_name.find_last_of('\\');
	if(last_slash != std::string::npos) {
		replay_name.erase(0, last_slash + 1);
	}
	
	log_push("Preparing to capture " + replay_name + "...\r\n");
	
	size_t last_dot = replay_name.find_last_of('.');
	if(last_dot != std::string::npos) {
		replay_name.erase(last_dot);
	}
	
	capture_path = wa_path + "\\User\\Capture\\" + replay_name;
	
	delete_frames(capture_path);
	CreateDirectory(capture_path.c_str(), NULL);
	
	if(config.enable_audio) {
		assert((audio_event = CreateEvent(NULL, FALSE, FALSE, NULL)));
		audio_rec = new audio_recorder(config, audio_event);
		
		wav_out = new wav_writer(capture_path + "\\" + FRAME_PREFIX + "audio.wav", CHANNELS, SAMPLE_RATE, SAMPLE_BITS);
	}
	
	recorded_frames = 0;
	last_frame_count = 0;
	
	/* Set WA options */
	
	orig_detail_level = wa_options.get_dword("DetailLevel", 0);
	wa_options.set_dword("DetailLevel", conf.wa_detail_level);
	
	orig_disable_phone = wa_options.get_dword("DisablePhone", 0);
	wa_options.set_dword("DisablePhone", (conf.wa_chat_behaviour == 1 ? 0 : 1));
	
	orig_chat_pinned = wa_options.get_dword("ChatPinned", 0);
	wa_options.set_dword("ChatPinned", (conf.wa_chat_behaviour == 2 ? 1 : 0));
	
	log_push("Starting WA...\r\n");
	
	std::string cmdline = "\"" + wa_path + "\\wa.exe\" /getvideo"
		" \"" + replay_path + "\""
		" \"" + to_string((double)50 / (double)config.frame_rate) + "\""
		" \"" + start + "\" \"" + end + "\""
		" \"" + to_string(config.width) + "\" \"" + to_string(config.height) + "\""
		" \"" + FRAME_PREFIX + "\"";
	
	worms_cmdline = new char[cmdline.length() + 1];
	strcpy(worms_cmdline, cmdline.c_str());
	
	STARTUPINFO sinfo;
	memset(&sinfo, 0, sizeof(sinfo));
	sinfo.cb = sizeof(sinfo);
	
	PROCESS_INFORMATION pinfo;
	
	assert(CreateProcess(NULL, worms_cmdline, NULL, NULL, FALSE, 0, NULL, NULL, &sinfo, &pinfo));
	
	worms_process = pinfo.hProcess;
	CloseHandle(pinfo.hThread);
	
	/* Create worker thread */
	
	assert((force_exit = CreateEvent(NULL, FALSE, FALSE, NULL)));
	assert((worker_thread = CreateThread(NULL, 0, &capture_worker_init, this, 0, NULL)));
}

wa_capture::~wa_capture() {
	SetEvent(force_exit);
	
	WaitForSingleObject(worker_thread, INFINITE);
	CloseHandle(worker_thread);
	
	CloseHandle(force_exit);
	
	TerminateProcess(worms_process, 1);
	WaitForSingleObject(worms_process, INFINITE);
	
	CloseHandle(worms_process);
	delete worms_cmdline;
	
	/* Restore original WA options */
	
	wa_options.set_dword("DetailLevel", orig_detail_level);
	wa_options.set_dword("DisablePhone", orig_disable_phone);
	wa_options.set_dword("ChatPinned", orig_chat_pinned);
	
	if(config.enable_audio) {
		delete wav_out;
		
		delete audio_rec;
		CloseHandle(audio_event);
	}
}

void wa_capture::worker_main() {
	HANDLE events[] = {force_exit, worms_process, audio_event};
	int n_events = 2;
	
	/* Audio recording isn't started until the worker thread starts executing
	 * to reduce the chance of the buffers running out and causing the multimedia
	 * API to automatically stop recording.
	*/
	
	if(config.enable_audio) {
		audio_rec->start();
		n_events = 3;
	}
	
	while(1) {
		DWORD wait = WaitForMultipleObjects(n_events, events, FALSE, INFINITE);
		
		switch(wait) {
			case WAIT_OBJECT_0: {
				return;
			}
			
			case WAIT_OBJECT_0 + 2: {
				flush_audio();
				break;
			}
			
			case WAIT_OBJECT_0 + 1: {
				/* Worms has exited, stop recording audio and flush buffers. */
				
				if(config.enable_audio) {
					audio_rec->stop();
					flush_audio();
					
					delete wav_out;
					wav_out = NULL;
					
					delete audio_rec;
					audio_rec = NULL;
				}
				
				DWORD exit_code;
				GetExitCodeProcess(worms_process, &exit_code);
				
				log_push("WA exited with status " + to_string(exit_code) + "\r\n");
				
				/* Notify progress dialog that capture is finished */
				PostMessage(progress_dialog, WM_WAEXIT, 0, 0);
				
				return;
			}
		}
	}
}

bool wa_capture::frame_exists(unsigned int frame) {
	char num_buf[16];
	sprintf(num_buf, "%06d", frame);
	
	std::string path = capture_path + "\\" + FRAME_PREFIX + num_buf + ".png";
	
	return (GetFileAttributes(path.c_str()) != INVALID_FILE_ATTRIBUTES);
}

unsigned int wa_capture::count_frames() {
	while(frame_exists(last_frame_count)) {
		last_frame_count++;
	}
	
	return last_frame_count;
}

void wa_capture::flush_audio() {
	size_t frames = count_frames();
	
	std::list<WAVEHDR> tmp_buffers = audio_rec->get_buffers();
	
	audio_buffers.insert(audio_buffers.end(), tmp_buffers.begin(), tmp_buffers.end());
	
	if(frames == recorded_frames || audio_buffers.empty()) {
		/* No new frames dumped or no audio available */
		return;
	}
	
	size_t frame_bytes = (wav_out->sample_rate * wav_out->sample_size) / config.frame_rate;
	
	while(frames > recorded_frames) {
		ssize_t buf_start = frames;
		
		for(std::list<WAVEHDR>::reverse_iterator b = audio_buffers.rbegin(); b != audio_buffers.rend();) {
			buf_start -= b->dwBytesRecorded / frame_bytes;
			
			size_t p_buf_start = buf_start >= 0 ? buf_start : 0;
			
			if(p_buf_start > recorded_frames) {
				if(++b == audio_buffers.rend()) {
					/* There's a gap in the audio stream, or WA is exporting
					 * frames faster than it should.
					*/
					
					/* Allow n frames of leeway to allow for frame writing being
					 * out of sync.
					*/
					
					if(recorded_frames + ALLOWED_AUDIO_SKEW >= p_buf_start) {
						return;
					}
					
					size_t pad_bytes = (p_buf_start - recorded_frames) * frame_bytes;
					
					char *zbuf = new char[pad_bytes];
					memset(zbuf, 0, pad_bytes);
					
					wav_out->append_data(zbuf, pad_bytes);
					recorded_frames += p_buf_start - recorded_frames;
					
					delete zbuf;
					
					b--;
				}else{
					continue;
				}
			}
			
			/* The needed audio may be further in due to lag */
			
			assert((ssize_t)recorded_frames >= buf_start);
			
			size_t skip_bytes = (recorded_frames - buf_start) * frame_bytes;
			
			wav_out->append_data(b->lpData + skip_bytes, b->dwBytesRecorded - skip_bytes);
			recorded_frames += (b->dwBytesRecorded - skip_bytes) / frame_bytes;
			
			break;
		}
	}
	
	/* Purge old buffers */
	
	for(std::list<WAVEHDR>::iterator b = audio_buffers.begin(); b != audio_buffers.end(); b++) {
		delete b->lpData;
	}
	
	audio_buffers.clear();
}
