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

typedef BOOL WINAPI (*CreateProcessExA_ptr)(LPCTSTR,LPTSTR,LPSECURITY_ATTRIBUTES,LPSECURITY_ATTRIBUTES,BOOL,DWORD,LPVOID,LPCTSTR,LPSTARTUPINFO,LPPROCESS_INFORMATION,LPCTSTR);

void delete_frames(const std::string &dir);
void log_push(const std::string &msg);

extern std::string wa_path;

extern HWND progress_dialog;

static DWORD WINAPI capture_worker_init(LPVOID this_ptr) {
	wa_capture *t = (wa_capture*)this_ptr;
	t->worker_main();
	
	return 0;
}

wa_capture::wa_capture(const std::string &replay, const arec_config &conf): config(conf) {
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
		
		wav_out = new wav_writer(config, capture_path + "\\" + FRAME_PREFIX + (config.do_second_pass ? "pass1.wav" : "audio.wav"));
	}
	
	recorded_frames = 0;
	last_frame_count = 0;
	
	this_pass = 1;
	
	/* Set WA options */
	
	set_option("DetailLevel", conf.wa_detail_level, 5);
	set_option("DisablePhone", (conf.wa_chat_behaviour == 1 ? 0 : 1));
	set_option("ChatPinned", (conf.wa_chat_behaviour == 2 ? 1 : 0));
	set_option("HomeLock", config.wa_lock_camera);
	set_option("LargerFonts", config.wa_bigger_fonts);
	set_option("InfoTransparency", config.wa_transparent_labels);
	
	std::string cmdline = "\"" + wa_path + "\\WA.exe\" /getvideo"
		" \"" + replay_path + "\""
		" \"" + to_string((double)50 / (double)config.frame_rate) + "\""
		" \"" + config.start_time + "\" \"" + config.end_time + "\""
		" \"" + to_string(config.width) + "\" \"" + to_string(config.height) + "\""
		" \"" + FRAME_PREFIX + "\"";
	
	madchook = NULL;
	start_wa(cmdline);
	
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
	
	if(madchook) {
		FreeLibrary(madchook);
	}
	
	/* Restore original WA options */
	
	for(std::map<std::string,DWORD>::iterator i = original_options.begin(); i != original_options.end(); i++) {
		wa_options.set_dword(i->first.c_str(), i->second);
	}
	
	if(config.enable_audio) {
		delete wav_out;
		
		delete audio_rec;
		CloseHandle(audio_event);
	}
}

static inline bool memeq(void *ptr_a, void *ptr_b, size_t size) {
	size_t off = 0;
	
	while(off < size) {
		if((size + off) % 4 == 0 && size - off >= 4) {
			if(*(uint32_t*)(((char*)ptr_a) + off) != *(uint32_t*)(((char*)ptr_b) + off)) {
				return false;
			}
			
			off += 4;
		}else if((size + off) % 2 == 0 && size - off >= 2) {
			if(*(uint16_t*)(((char*)ptr_a) + off) != *(uint16_t*)(((char*)ptr_b) + off)) {
				return false;
			}
			
			off += 2;
		}else{
			if(*(uint8_t*)(((char*)ptr_a) + off) != *(uint8_t*)(((char*)ptr_b) + off)) {
				return false;
			}
			
			off++;
		}
	}
	
	return true;
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
				
				if(config.enable_audio && config.do_second_pass) {
					if(this_pass == 1) {
						/* Start second pass for non-laggy audio */
						
						this_pass = 2;
						
						log_push("Starting second pass...\r\n");
						
						CloseHandle(worms_process);
						delete worms_cmdline;
						
						audio_rec = new audio_recorder(config, audio_event);
						wav_out = new wav_writer(config, capture_path + "\\" + FRAME_PREFIX + "pass2.wav");
						
						std::string cmdline = "\"" + wa_path + "\\WA.exe\"";
						
						if(!config.start_time.empty()) {
							cmdline.append(" /playat");
						}
						
						cmdline.append(std::string(" \"") + replay_path + "\"");
						
						if(!config.start_time.empty()) {
							cmdline.append(std::string(" \"") + config.start_time + "\"");
						}
						
						start_wa(cmdline);
						events[1] = worms_process;
						
						audio_rec->start();
						
						break;
					}else{
						/* Second pass complete, create sync'd wav file */
						
						log_push("Searching for audio offset...\r\n");
						
						wav_reader pass1(capture_path + "\\" + FRAME_PREFIX + "pass1.wav");
						wav_reader pass2(capture_path + "\\" + FRAME_PREFIX + "pass2.wav");
						
						size_t buf_samples = config.audio_rate * PASS_SYNC_BUF_SECS;
						size_t buf_size = pass1.sample_size * buf_samples;
						
						char *pcm_buf = new char[buf_size];
						
						size_t p1_samples = pass1.read_samples(pcm_buf, buf_samples);
						std::vector<int16_t> p1_avgs = gen_averages(pcm_buf, p1_samples, 32767);
						
						size_t p2_samples = pass2.read_samples(pcm_buf, buf_samples);
						std::vector<int16_t> p2_avgs = gen_averages(pcm_buf, p2_samples, -32768);
						
						size_t p1_max = p1_avgs.size() - PASS_SYNC_CMP_FRAMES / PASS_SYNC_MEAN_FRAMES;
						size_t p2_max = p2_avgs.size() - PASS_SYNC_CMP_FRAMES / PASS_SYNC_MEAN_FRAMES;
						
						size_t off_multi = (config.audio_rate / config.frame_rate) * PASS_SYNC_MEAN_FRAMES;
						
						size_t best_off = 0, best_variation = (size_t)(-1);
						
						for(size_t p1_off = 0; p1_off <= p1_max; p1_off++) {
							for(size_t p2_off = 0; p2_off <= p2_max; p2_off++) {
								unsigned int variation = calc_variation(p1_avgs, p1_off, p2_avgs, p2_off);
								
								if(variation < best_variation) {
									best_variation = variation;
									best_off = (p1_off > p2_off ? p1_off - p2_off : p2_off - p1_off) * off_multi;
								}
							}
						}
						
						log_push("Best pass 2 offset appears to be " + to_string(best_off) + "\r\n");
						
						log_push("Writing audio.wav...\r\n");
						
						wav_out = new wav_writer(config, capture_path + "\\" + FRAME_PREFIX + "audio.wav");
						
						pass2.reset();
						pass2.skip_samples(best_off);
						
						size_t samples, write_samples = (config.audio_rate / config.frame_rate) * count_frames();
						
						while(write_samples && (samples = pass2.read_samples(pcm_buf, buf_samples))) {
							if(samples > write_samples) {
								samples = write_samples;
								write_samples = 0;
							}else{
								write_samples -= samples;
							}
							
							wav_out->append_data(pcm_buf, samples * pass2.sample_size);
						}
						
						delete wav_out;
						wav_out = NULL;
						
						delete pcm_buf;
					}
				}
				
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
	if(this_pass == 2) {
		pass2_flush();
		return;
	}
	
	size_t frames = count_frames();
	
	std::list<WAVEHDR> tmp_buffers = audio_rec->get_buffers();
	
	audio_buffers.insert(audio_buffers.end(), tmp_buffers.begin(), tmp_buffers.end());
	
	if(frames == recorded_frames || audio_buffers.empty()) {
		/* No new frames dumped or no audio available */
		return;
	}
	
	size_t frame_bytes = (wav_out->sample_rate * wav_out->sample_size) / config.frame_rate;
	size_t frame_samples = wav_out->sample_rate / config.frame_rate;
	
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
					
					if(recorded_frames + config.max_skew >= p_buf_start) {
						return;
					}
					
					wav_out->extend_sample((p_buf_start - recorded_frames) * frame_samples);
					recorded_frames += p_buf_start - recorded_frames;
					
					/*
					size_t pad_bytes = (p_buf_start - recorded_frames) * frame_bytes;
					
					char *zbuf = new char[pad_bytes];
					memset(zbuf, 0, pad_bytes);
					
					wav_out->append_data(zbuf, pad_bytes);
					recorded_frames += p_buf_start - recorded_frames;
					
					delete zbuf;
					*/
					
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

void wa_capture::pass2_flush() {
	std::list<WAVEHDR> buffers = audio_rec->get_buffers();
	
	for(std::list<WAVEHDR>::iterator b = buffers.begin(); b != buffers.end(); b++) {
		wav_out->append_data(b->lpData, b->dwBytesRecorded);
	}
}

void wa_capture::set_option(const char *name, DWORD value, DWORD def_value) {
	original_options.insert(std::make_pair<const char*,DWORD>(name, wa_options.get_dword(name, def_value)));
	wa_options.set_dword(name, value);
}

void wa_capture::start_wa(const std::string &cmdline) {
	worms_cmdline = new char[cmdline.length() + 1];
	strcpy(worms_cmdline, cmdline.c_str());
	
	STARTUPINFO sinfo;
	memset(&sinfo, 0, sizeof(sinfo));
	sinfo.cb = sizeof(sinfo);
	
	PROCESS_INFORMATION pinfo;
	
	if(wormkit_present && config.load_wormkit_dlls) {
		log_push("Loading madCHook...\r\n");
		assert(madchook || (madchook = LoadLibrary(std::string(wa_path + "\\madCHook.dll").c_str())));
		
		CreateProcessExA_ptr CreateProcessExA = (CreateProcessExA_ptr)GetProcAddress(madchook, "CreateProcessExA");
		assert(CreateProcessExA);
		
		log_push("Starting WA...\r\n");
		assert(CreateProcessExA(NULL, worms_cmdline, NULL, NULL, FALSE, 0, NULL, NULL, &sinfo, &pinfo, std::string(wa_path + "\\HookLib.dll").c_str()));
	}else{
		log_push("Starting WA...\r\n");
		assert(CreateProcess(NULL, worms_cmdline, NULL, NULL, FALSE, 0, NULL, NULL, &sinfo, &pinfo));
	}
	
	worms_process = pinfo.hProcess;
	CloseHandle(pinfo.hThread);
}

std::vector<int16_t> wa_capture::gen_averages(char *raw_pcm, size_t samples, int16_t dead_val) {
	int avg = 0;
	size_t avg_count = 0;
	
	samples *= config.audio_channels;
	
	size_t sample_size = config.audio_bits / 8;
	size_t samples_per_avg = (config.audio_rate / config.frame_rate) * PASS_SYNC_MEAN_FRAMES * config.audio_channels;
	
	std::vector<int16_t> averages;
	
	for(size_t s = 0; s + sample_size <= samples; s++) {
		int sample = (sample_size == 1 ? *(uint8_t*)(raw_pcm + s * sample_size) : *(int16_t*)(raw_pcm + s * sample_size));
		
		/* Replace any samples below a certain threshold with dead_val, used to reduce
		 * the chance of synchronizing on background music or silence.
		*/
		
		if((sample_size == 1 && (sample > PASS_SYNC_DEAD_8_MAX || sample < PASS_SYNC_DEAD_8_MIN)) || (sample_size == 2 && (sample > PASS_SYNC_DEAD_16_MAX || sample < PASS_SYNC_DEAD_16_MIN))) {
			avg += sample;
		}else{
			avg += dead_val;
		}
		
		if(++avg_count == samples_per_avg) {
			averages.push_back(avg / avg_count);
			
			avg = 0;
			avg_count = 0;
		}
	}
	
	return averages;
}

unsigned int wa_capture::calc_variation(const std::vector<int16_t> &a, size_t a_min, const std::vector<int16_t> &b, size_t b_min) {
	uint64_t var = 0;
	unsigned int var_count = 0;
	
	size_t max = PASS_SYNC_CMP_FRAMES / PASS_SYNC_MEAN_FRAMES;
	
	size_t ap = a_min, bp = b_min;
	
	for(; ap < a.size() && bp < b.size() && max--; ap++, bp++) {
		int low = (a[ap] < b[bp] ? a[ap] : b[bp]);
		int high = (a[ap] > b[bp] ? a[ap] : b[bp]);
		
		var += high - low;
		var_count++;
	}
	
	return var / var_count;
}
