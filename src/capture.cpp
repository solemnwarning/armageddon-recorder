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

extern bool enable_audio;
extern unsigned int audio_source;

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

wa_capture::wa_capture(const std::string &replay, unsigned int width, unsigned int height, unsigned int fps, const std::string &start, const std::string &end) {
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
	
	frame_rate = fps;
	
	delete_frames(capture_path);
	CreateDirectory(capture_path.c_str(), NULL);
	
	assert((audio_event = CreateEvent(NULL, FALSE, FALSE, NULL)));
	
	if(enable_audio) {
		using_rec_a = true;
		audio_rec_a = new audio_recorder(audio_source, audio_event);
		audio_rec_b = new audio_recorder(audio_source, audio_event);
		
		wav_out = new wav_writer(capture_path + "\\" + FRAME_PREFIX + "audio.wav", CHANNELS, SAMPLE_RATE, SAMPLE_BITS);
		next_sync = 0;
	}else{
		audio_rec_a = audio_rec_b = NULL;
		wav_out = NULL;
	}
	
	/* Monitor capture directory for creation of frames */
	capture_monitor = FindFirstChangeNotification(capture_path.c_str(), FALSE, FILE_NOTIFY_CHANGE_FILE_NAME);
	assert(capture_monitor != INVALID_HANDLE_VALUE);
	
	log_push("Starting WA...\r\n");
	
	std::string cmdline = "\"" + wa_path + "\\wa.exe\" /getvideo"
		" \"" + replay_path + "\""
		" \"" + to_string((double)50 / (double)frame_rate) + "\""
		" \"" + start + "\" \"" + end + "\""
		" \"" + to_string(width) + "\" \"" + to_string(height) + "\""
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
	
	FindCloseChangeNotification(capture_monitor);
	
	delete wav_out;
	
	delete audio_rec_a;
	delete audio_rec_b;
	
	CloseHandle(audio_event);
}

void wa_capture::worker_main() {
	HANDLE events[] = {force_exit, audio_event, worms_process, capture_monitor};
	int n_events = 4;
	
	while(1) {
		DWORD wait = WaitForMultipleObjects(n_events, events, FALSE, INFINITE);
		
		switch(wait) {
			case WAIT_OBJECT_0: {
				return;
			}
			
			case WAIT_OBJECT_0 + 1: {
				flush_audio(audio_rec_a);
				flush_audio(audio_rec_b);
				
				break;
			}
			
			case WAIT_OBJECT_0 + 2: {
				/* Worms has exited, stop recording audio, flush buffers and
				 * truncate WAV file to correct length.
				*/
				
				if(enable_audio) {
					audio_rec_a->stop();
					flush_audio(audio_rec_a);
					
					delete audio_rec_a;
					audio_rec_a = NULL;
					
					audio_rec_b->stop();
					flush_audio(audio_rec_b);
					
					delete audio_rec_b;
					audio_rec_b = NULL;
					
					/* Truncate WAV file to length */
					
					unsigned int num_frames = count_frames();
					
					wav_out->force_length((SAMPLE_RATE / frame_rate) * num_frames);
					
					delete wav_out;
					wav_out = NULL;
				}
				
				DWORD exit_code;
				GetExitCodeProcess(worms_process, &exit_code);
				
				log_push("WA exited with status " + to_string(exit_code) + "\r\n");
				
				/* Notify progress dialog that capture is finished */
				PostMessage(progress_dialog, WM_WAEXIT, 0, 0);
				
				return;
			}
			
			case WAIT_OBJECT_0 + 3: {
				if(!enable_audio) {
					n_events--;
					break;
				}
				
				size_t frames = count_frames();
				
				if(frames > next_sync) {
					audio_recorder *o_rec = (using_rec_a ? audio_rec_a : audio_rec_b);
					audio_recorder *n_rec = (using_rec_a ? audio_rec_b : audio_rec_a);
					
					using_rec_a = !using_rec_a;
					
					n_rec->start();
					o_rec->stop();
					
					next_sync = frames + SYNC_FRAMES;
					
					flush_audio(o_rec);
					
					wav_out->force_length((SAMPLE_RATE / frame_rate) * (frames + 1));
					
					FindNextChangeNotification(capture_monitor);
					
					/*
					audio_rec->start();
					n_events--;
					*/
				}
				
				#if 0
				if(GetFileAttributes(frame_path.c_str()) != INVALID_FILE_ATTRIBUTES) {
					/* Frame #0 exits, start recording audio and ignore further
					 * events from capture_monitor.
					*/
					
					audio_rec->start();
					n_events--;
				}else{
					FindNextChangeNotification(capture_monitor);
				}
				#endif
				
				break;
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
	unsigned int frames = 0;
	
	while(frame_exists(frames)) {
		frames++;
	}
	
	return frames;
}

void wa_capture::flush_audio(audio_recorder *rec) {
	std::list<WAVEHDR> buffers = rec->get_buffers();
	
	for(std::list<WAVEHDR>::iterator b = buffers.begin(); b != buffers.end(); b++) {
		wav_out->append_data(b->lpData, b->dwBytesRecorded);
		delete b->lpData;
	}
}
