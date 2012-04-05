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

#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <objbase.h>
#include <shlobj.h>
#include <vfw.h>
#include <list>
#include <string>
#include <iostream>
#include <vector>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <stdint.h>

#include "resource.h"
#include "audio.hpp"
#include "reg.hpp"
#include "encode.hpp"
#include "capture.hpp"
#include "main.hpp"

/* I thought we were past missing things, MinGW... */
#define BIF_NONEWFOLDERBUTTON 0x00000200
typedef LPITEMIDLIST PIDLIST_ABSOLUTE;

const char *detail_levels[] = {
	"0 - No background",
	"1 - Extra waves",
	"2 - Gradient background, more waves",
	"3 - Smoother gradient background",
	"4 - Flying debris in background",
	"5 - Images in background",
	NULL
};

const char *chat_levels[] = {
	"Show nothing",
	"Show telephone",
	"Show messages",
	NULL
};

std::string replay_path;

arec_config config;

std::string video_path;
unsigned int video_format = 2;
unsigned int audio_format;

bool do_cleanup;

std::string wa_path;
bool wormkit_exe, wormkit_ds;

HWND progress_dialog = NULL;
bool com_init = false;		/* COM has been initialized in the main thread */

reg_handle wa_options(HKEY_CURRENT_USER, "Software\\Team17SoftwareLTD\\WormsArmageddon\\Options", KEY_QUERY_VALUE | KEY_SET_VALUE, false);

INT_PTR CALLBACK options_dproc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);

std::string get_window_string(HWND hwnd) {
	char buf[1024];
	
	GetWindowText(hwnd, buf, sizeof(buf));
	return buf;
}

size_t get_window_uint(HWND window) {
	std::string s = get_window_string(window);
	
	if(s.empty() || strspn(s.c_str(), "1234567890") != s.length()) {
		return -1;
	}
	
	return strtoul(s.c_str(), NULL, 10);
}

double get_window_double(HWND window) {
	std::string s = get_window_string(window);
	
	if(s.empty() || strspn(s.c_str(), "1234567890.") != s.length()) {
		return -1;
	}
	
	return atof(s.c_str());
}

/* Test if a start/end time is in valid format. An empty string is valid */
bool validate_time(const std::string &time) {
	int stage = 0;
	
	for(size_t i = 0; i < time.length();) {
		if((stage == 1 || stage == 3) && time[i] == '.') {
			stage = 5;
		}
		
		switch(stage) {
			case 0:
			case 2:
			case 4:
			case 6:
				if(isdigit(time[i++])) {
					while(i < time.length() && isdigit(time[i])) {
						i++;
					}
					
					stage++;
				}else{
					return false;
				}
				
				break;
				
			case 1:
			case 3:
				if(time[i++] == ':' && i < time.length() && isdigit(time[i])) {
					stage++;
				}else{
					return false;
				}
				
				break;
				
			case 5:
				if(time[i++] == '.' && i < time.length() && isdigit(time[i])) {
					stage++;
				}else{
					return false;
				}
				
				break;
				
			default:
				return false;
		}
	}
	
	return true;
}

bool validate_res(const std::string &value) {
	return (!value.empty() && value.find_first_not_of("1234567890") == std::string::npos);
}

bool validate_fps(const std::string &value) {
	return (validate_res(value) && atoi(value.c_str()) >= 1 && atoi(value.c_str()) <= 50);
}

/* TODO: Default path */
std::string choose_dir(HWND parent, const std::string &title, const std::string &test_file) {
	if(!com_init) {
		HRESULT err = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
		if(err != S_OK) {
			/* TODO: Error message */
			MessageBox(parent, std::string("CoInitializeEx: " + to_string(err)).c_str(), NULL, MB_OK | MB_ICONERROR);
			return std::string();
		}
		
		com_init = true;
	}
	
	std::string ret;
	
	while(ret.empty()) {
		BROWSEINFO binfo;
		memset(&binfo, 0, sizeof(binfo));
		
		binfo.hwndOwner = parent;
		binfo.lpszTitle = title.c_str();
		binfo.ulFlags = BIF_NONEWFOLDERBUTTON | BIF_RETURNONLYFSDIRS;
		
		PIDLIST_ABSOLUTE pidl = SHBrowseForFolder(&binfo);
		if(pidl) {
			char path[MAX_PATH];
			std::string test_path;
			
			if(!SHGetPathFromIDList(pidl, path)) {
				goto NOT_FOUND;
			}
			
			CoTaskMemFree(pidl);
			
			if(path[strlen(path) - 1] == '\\') {
				path[strlen(path) - 1] = '\0';
			}
			
			test_path = std::string(path) + "\\" + test_file;
			
			if(GetFileAttributes(test_path.c_str()) != INVALID_FILE_ATTRIBUTES) {
				ret = path;
				break;
			}
			
			NOT_FOUND:
			MessageBox(parent, std::string("Selected directory does not contain " + test_file).c_str(), NULL, MB_OK | MB_ICONERROR);
		}else{
			break;
		}
	}
	
	return ret;
}

void set_combo_height(HWND combo) {
	RECT rect;
	
	GetWindowRect(combo, &rect);
	SetWindowPos(combo, 0, 0, 0, rect.right - rect.left, LIST_HEIGHT, SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOZORDER);
}

void check_wormkit() {
	wormkit_exe = (
		GetFileAttributes(std::string(wa_path + "\\madCHook.dll").c_str()) != INVALID_FILE_ATTRIBUTES &&
		GetFileAttributes(std::string(wa_path + "\\HookLib.dll").c_str()) != INVALID_FILE_ATTRIBUTES
	);
	
	wormkit_ds = (GetFileAttributes(std::string(wa_path + "\\dsound.dll").c_str()) != INVALID_FILE_ATTRIBUTES);
}

INT_PTR CALLBACK main_dproc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
	switch(msg) {
		case WM_INITDIALOG: {
			SendMessage(hwnd, WM_SETICON, 0, (LPARAM)LoadIcon(GetModuleHandle(NULL), MAKEINTRESOURCE(ICON16)));
			SendMessage(hwnd, WM_SETICON, 1, (LPARAM)LoadIcon(GetModuleHandle(NULL), MAKEINTRESOURCE(ICON32)));
			
			EnableMenuItem(GetMenu(hwnd), LOAD_WORMKIT_DLLS, (wormkit_exe && !wormkit_ds) ? MF_ENABLED : MF_GRAYED);
			CheckMenuItem(GetMenu(hwnd), LOAD_WORMKIT_DLLS, (wormkit_ds || (wormkit_exe && config.load_wormkit_dlls)) ? MF_CHECKED : MF_UNCHECKED);
			
			CheckMenuItem(GetMenu(hwnd), DO_SECOND_PASS, config.do_second_pass ? MF_CHECKED : MF_UNCHECKED);
			
			SetWindowText(GetDlgItem(hwnd, RES_X), to_string(config.width).c_str());
			SetWindowText(GetDlgItem(hwnd, RES_Y), to_string(config.height).c_str());
			
			HWND fmt_list = GetDlgItem(hwnd, VIDEO_FORMAT);
			
			for(unsigned int i = 0; i < encoders.size(); i++) {
				ComboBox_AddString(fmt_list, encoders[i].name.c_str());
			}
			
			ComboBox_SetCurSel(fmt_list, video_format);
			set_combo_height(fmt_list);
			
			HWND audio_list = GetDlgItem(hwnd, AUDIO_SOURCE);
			
			ComboBox_AddString(audio_list, "None");
			ComboBox_SetCurSel(audio_list, 0);
			
			for(unsigned int i = 0; i < audio_sources.size(); i++) {
				ComboBox_AddString(audio_list, audio_sources[i].szPname);
				
				if(config.enable_audio && (i == config.audio_source || i == 0)) {
					ComboBox_SetCurSel(audio_list, i + 1);
				}
			}
			
			set_combo_height(audio_list);
			
			SetWindowText(GetDlgItem(hwnd, FRAMES_SEC), to_string(config.frame_rate).c_str());
			
			HWND detail_list = GetDlgItem(hwnd, WA_DETAIL);
			
			for(unsigned int i = 0; detail_levels[i]; i++) {
				ComboBox_AddString(detail_list, detail_levels[i]);
			}
			
			ComboBox_SetCurSel(detail_list, config.wa_detail_level);
			set_combo_height(detail_list);
			
			HWND chat_list = GetDlgItem(hwnd, WA_CHAT);
			
			for(unsigned int i = 0; chat_levels[i]; i++) {
				ComboBox_AddString(chat_list, chat_levels[i]);
			}
			
			ComboBox_SetCurSel(chat_list, config.wa_chat_behaviour);
			set_combo_height(chat_list);
			
			CheckMenuItem(GetMenu(hwnd), WA_LOCK_CAMERA, config.wa_lock_camera ? MF_CHECKED : MF_UNCHECKED);
			CheckMenuItem(GetMenu(hwnd), WA_BIGGER_FONT, config.wa_bigger_fonts ? MF_CHECKED : MF_UNCHECKED);
			CheckMenuItem(GetMenu(hwnd), WA_TRANSPARENT_LABELS, config.wa_transparent_labels ? MF_CHECKED : MF_UNCHECKED);
			
			HWND audio_fmt_list = GetDlgItem(hwnd, AUDIO_FORMAT_MENU);
			
			for(unsigned int i = 0; audio_encoders[i].name; i++) {
				ComboBox_AddString(audio_fmt_list, audio_encoders[i].desc);
			}
			
			ComboBox_SetCurSel(audio_fmt_list, audio_format);
			set_combo_height(audio_fmt_list);
			
			Button_SetCheck(GetDlgItem(hwnd, DO_CLEANUP), (do_cleanup ? BST_CHECKED : BST_UNCHECKED));
			
			goto VIDEO_ENABLE;
			
			return TRUE;
		}
		
		case WM_CLOSE: {
			EndDialog(hwnd, 0);
			return TRUE;
		}
		
		case WM_COMMAND: {
			if(HIWORD(wp) == BN_CLICKED) {
				switch(LOWORD(wp)) {
					case IDOK: {
						replay_path = get_window_string(GetDlgItem(hwnd, REPLAY_PATH));
						video_path = get_window_string(GetDlgItem(hwnd, AVI_PATH));
						video_format = ComboBox_GetCurSel(GetDlgItem(hwnd, VIDEO_FORMAT));
						audio_format = ComboBox_GetCurSel(GetDlgItem(hwnd, AUDIO_FORMAT_MENU));
						
						config.audio_source = ComboBox_GetCurSel(GetDlgItem(hwnd, AUDIO_SOURCE));
						config.enable_audio = (config.audio_source-- > 0);
						
						if(config.enable_audio && !test_audio_format(config.audio_source, config.audio_rate, config.audio_channels, config.audio_bits)) {
							MessageBox(hwnd, "Selected audio format (rate, channels, bits) is not supported by audio device", NULL, MB_OK | MB_ICONERROR);
							break;
						}
						
						std::string rx_string = get_window_string(GetDlgItem(hwnd, RES_X));
						std::string ry_string = get_window_string(GetDlgItem(hwnd, RES_Y));
						
						if(!validate_res(rx_string) || !validate_res(ry_string)) {
							MessageBox(hwnd, "Invalid resolution", NULL, MB_OK | MB_ICONERROR);
							break;
						}
						
						config.width = strtoul(rx_string.c_str(), NULL, 10);
						config.height = strtoul(ry_string.c_str(), NULL, 10);
						
						std::string fps_text = get_window_string(GetDlgItem(hwnd, FRAMES_SEC));
						
						if(!validate_fps(fps_text)) {
							MessageBox(hwnd, "Frame rate must be an integer in the range 1-50", NULL, MB_OK | MB_ICONERROR);
							break;
						}
						
						config.frame_rate = atoi(fps_text.c_str());
						
						config.start_time = get_window_string(GetDlgItem(hwnd, TIME_START));
						config.end_time = get_window_string(GetDlgItem(hwnd, TIME_END));
						
						if(!validate_time(config.start_time)) {
							MessageBox(hwnd, "Invalid start time", NULL, MB_OK | MB_ICONERROR);
							break;
						}
						
						if(!validate_time(config.end_time)) {
							MessageBox(hwnd, "Invalid end time", NULL, MB_OK | MB_ICONERROR);
							break;
						}
						
						config.wa_detail_level = ComboBox_GetCurSel(GetDlgItem(hwnd, WA_DETAIL));
						config.wa_chat_behaviour = ComboBox_GetCurSel(GetDlgItem(hwnd, WA_CHAT));
						
						do_cleanup = Button_GetCheck(GetDlgItem(hwnd, DO_CLEANUP));
						
						if(video_format == 0 && do_cleanup) {
							MessageBox(hwnd, "You've chosen to not create a video file and delete frames/audio when finished. You probably don't want this.", NULL, MB_OK | MB_ICONWARNING);
							break;
						}
						
						if(video_format) {
							if(video_path.empty()) {
								MessageBox(hwnd, "Output video filename is required", NULL, MB_OK | MB_ICONERROR);
								break;
							}
						}
						
						EndDialog(hwnd, 1);
						break;
					}
					
					case IDCANCEL: {
						EndDialog(hwnd, 0);
						break;
					}
					
					case REPLAY_BROWSE: {
						char filename[512] = "";
						
						OPENFILENAME openfile;
						memset(&openfile, 0, sizeof(openfile));
						
						openfile.lStructSize = sizeof(openfile);
						openfile.hwndOwner = hwnd;
						openfile.lpstrFilter = "Worms Armageddon replay (*.WAgame)\0*.WAgame\0All Files\0*\0";
						openfile.lpstrFile = filename;
						openfile.nMaxFile = sizeof(filename);
						openfile.lpstrInitialDir = (config.replay_dir.length() ? config.replay_dir.c_str() : NULL);
						openfile.lpstrTitle = "Select replay";
						openfile.Flags = OFN_FILEMUSTEXIST | OFN_HIDEREADONLY;
						
						if(GetOpenFileName(&openfile)) {
							replay_path = filename;
							SetWindowText(GetDlgItem(hwnd, REPLAY_PATH), replay_path.c_str());
							
							config.replay_dir = replay_path;
							config.replay_dir.erase(config.replay_dir.find_last_of('\\'));
						}else if(CommDlgExtendedError()) {
							MessageBox(hwnd, std::string("GetOpenFileName: " + to_string(CommDlgExtendedError())).c_str(), NULL, MB_OK | MB_ICONERROR);
						}
						
						break;
					}
					
					case AVI_BROWSE: {
						char filename[512] = "";
						
						OPENFILENAME openfile;
						memset(&openfile, 0, sizeof(openfile));
						
						openfile.lStructSize = sizeof(openfile);
						openfile.hwndOwner = hwnd;
						openfile.lpstrFile = filename;
						openfile.nMaxFile = sizeof(filename);
						openfile.lpstrInitialDir = (config.video_dir.length() ? config.video_dir.c_str() : NULL);
						openfile.lpstrTitle = "Save video as...";
						openfile.Flags = OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT;
						openfile.lpstrDefExt = encoders[video_format].default_ext;
						
						if(GetSaveFileName(&openfile)) {
							video_path = filename;
							SetWindowText(GetDlgItem(hwnd, AVI_PATH), video_path.c_str());
							
							config.video_dir = video_path;
							config.video_dir.erase(config.video_dir.find_last_of('\\'));
						}else if(CommDlgExtendedError()) {
							MessageBox(hwnd, std::string("GetSaveFileName: " + to_string(CommDlgExtendedError())).c_str(), NULL, MB_OK | MB_ICONERROR);
						}
						
						break;
					}
					
					case SELECT_WA_DIR: {
						std::string dir = choose_dir(hwnd, "Select Worms Armageddon directory:", "wa.exe");
						if(!dir.empty()) {
							wa_path = dir;
							
							check_wormkit();
							
							EnableMenuItem(GetMenu(hwnd), LOAD_WORMKIT_DLLS, (wormkit_exe && !wormkit_ds) ? MF_ENABLED : MF_GRAYED);
							CheckMenuItem(GetMenu(hwnd), LOAD_WORMKIT_DLLS, (wormkit_ds || (wormkit_exe && config.load_wormkit_dlls)) ? MF_CHECKED : MF_UNCHECKED);
						}
						
						break;
					}
					
					case LOAD_WORMKIT_DLLS: {
						config.load_wormkit_dlls = !config.load_wormkit_dlls;
						CheckMenuItem(GetMenu(hwnd), LOAD_WORMKIT_DLLS, config.load_wormkit_dlls ? MF_CHECKED : MF_UNCHECKED);
						break;
					}
					
					case ADV_OPTIONS: {
						DialogBox(GetModuleHandle(NULL), MAKEINTRESOURCE(DLG_OPTIONS), hwnd, &options_dproc);
						break;
					}
					
					case DO_SECOND_PASS: {
						config.do_second_pass = !config.do_second_pass;
						CheckMenuItem(GetMenu(hwnd), DO_SECOND_PASS, config.do_second_pass ? MF_CHECKED : MF_UNCHECKED);
						break;
					}
					
					case WA_LOCK_CAMERA: {
						config.wa_lock_camera = !config.wa_lock_camera;
						CheckMenuItem(GetMenu(hwnd), WA_LOCK_CAMERA, config.wa_lock_camera ? MF_CHECKED : MF_UNCHECKED);
						break;
					}
					
					case WA_BIGGER_FONT: {
						config.wa_bigger_fonts = !config.wa_bigger_fonts;
						CheckMenuItem(GetMenu(hwnd), WA_BIGGER_FONT, config.wa_bigger_fonts ? MF_CHECKED : MF_UNCHECKED);
						break;
					}
					
					case WA_TRANSPARENT_LABELS: {
						config.wa_transparent_labels = !config.wa_transparent_labels;
						CheckMenuItem(GetMenu(hwnd), WA_TRANSPARENT_LABELS, config.wa_transparent_labels ? MF_CHECKED : MF_UNCHECKED);
						break;
					}
				}
				
				return TRUE;
			}else if(HIWORD(wp) == CBN_SELCHANGE) {
				if(LOWORD(wp) == VIDEO_FORMAT) {
					VIDEO_ENABLE:
					
					video_format = ComboBox_GetCurSel(GetDlgItem(hwnd, VIDEO_FORMAT));
					
					EnableWindow(GetDlgItem(hwnd, AVI_PATH), video_format != 0);
					EnableWindow(GetDlgItem(hwnd, AVI_BROWSE), video_format != 0);
					
					return TRUE;
				}
			}
		}
		
		default:
			break;
	}
	
	return FALSE;
}

#define WM_BEGIN WM_USER+3
#define WM_ENC_EXIT WM_USER+4

void delete_frames(const std::string &dir) {
	for(unsigned int i = 0;; i++) {
		char frame_n[16];
		snprintf(frame_n, sizeof(frame_n), "%06u", i);
		
		std::string file = dir + "\\" + FRAME_PREFIX + frame_n + ".png";
		
		if(GetFileAttributes(file.c_str()) != INVALID_FILE_ATTRIBUTES) {
			DeleteFile(file.c_str());
		}else{
			break;
		}
	}
}

std::string escape_filename(std::string name) {
	for(size_t i = 0; i < name.length(); i++) {
		if(name[i] == '\\') {
			name.insert(i++, "\\");
		}
	}
	
	return name;
}

struct exe_launcher {
	HWND window;
	UINT message;
	
	HANDLE worker;
	HANDLE worker_exit;
	
	char *cmdline_buf;
	HANDLE process;
	
	exe_launcher(const std::string &cmdline, HWND hwnd, UINT msg);
	~exe_launcher();
	
	DWORD main();
};

static WINAPI DWORD exe_launcher_init(LPVOID this_ptr) {
	exe_launcher *v = (exe_launcher*)this_ptr;
	return v->main();
}

exe_launcher::exe_launcher(const std::string &cmdline, HWND hwnd, UINT msg) {
	window = hwnd;
	message = msg;
	
	assert((worker_exit = CreateEvent(NULL, FALSE, FALSE, NULL)));
	
	cmdline_buf = new char[cmdline.length() + 1];
	strcpy(cmdline_buf, cmdline.c_str());
	
	STARTUPINFO sinfo;
	memset(&sinfo, 0, sizeof(sinfo));
	sinfo.cb = sizeof(sinfo);
	
	PROCESS_INFORMATION pinfo;
	
	assert(CreateProcess(NULL, cmdline_buf, NULL, NULL, FALSE, 0, NULL, NULL, &sinfo, &pinfo));
	
	process = pinfo.hProcess;
	CloseHandle(pinfo.hThread);
	
	assert((worker = CreateThread(NULL, 0, &exe_launcher_init, this, 0, NULL)));
}

exe_launcher::~exe_launcher() {
	SetEvent(worker_exit);
	
	WaitForSingleObject(worker, INFINITE);
	CloseHandle(worker);
	CloseHandle(worker_exit);
	
	TerminateProcess(process, 1);
	CloseHandle(process);
	
	delete cmdline_buf;
}

DWORD exe_launcher::main() {
	HANDLE events[] = {process, worker_exit};
	
	if(WaitForMultipleObjects(2, events, FALSE, INFINITE) == WAIT_OBJECT_0) {
		PostMessage(window, message, 0, 0);
	}
	
	return 0;
}

std::string ffmpeg_cmdline(const encoder_info &format, const std::string &capture_dir) {
	std::string frames_in = escape_filename(capture_dir + "\\" + FRAME_PREFIX + "%06d.png");
	std::string audio_in = escape_filename(capture_dir + "\\" + FRAME_PREFIX + "audio.wav");
	std::string video_out = escape_filename(video_path);
	
	std::string cmdline = "ffmpeg.exe -threads " + to_string(config.max_enc_threads) + " -y -r " + to_string(config.frame_rate) + " -i \"" + frames_in + "\"";
	
	if(config.enable_audio) {
		cmdline.append(std::string(" -i \"") + audio_in + "\"");
	}
	
	cmdline.append(std::string(" -vcodec ") + format.video_format + " -acodec " + audio_encoders[audio_format].name);
	
	if(format.bps_pix) {
		unsigned int bps = config.width * config.height;
		bps *= format.bps_pix;
		
		cmdline.append(std::string(" -b:v ") + to_string(bps));
	}
	
	cmdline.append(std::string(" \"") + video_out + "\"");
	
	return cmdline;
}

void log_push(const std::string &msg) {
	SendMessage(progress_dialog, WM_PUSHLOG, (WPARAM)&msg, 0);
}

INT_PTR CALLBACK prog_dproc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
	static wa_capture *capture = NULL;
	static std::string capture_path;
	
	static exe_launcher *encoder = NULL;
	
	static int return_code;
	
	switch(msg) {
		case WM_INITDIALOG: {
			progress_dialog = hwnd;
			return_code = 0;
			
			SendMessage(hwnd, WM_SETICON, 0, (LPARAM)LoadIcon(GetModuleHandle(NULL), MAKEINTRESOURCE(ICON16)));
			SendMessage(hwnd, WM_SETICON, 1, (LPARAM)LoadIcon(GetModuleHandle(NULL), MAKEINTRESOURCE(ICON32)));
			
			PostMessage(hwnd, WM_BEGIN, 0, 0);
			return TRUE;
		}
		
		case WM_CLOSE: {
			delete encoder;
			encoder = NULL;
			
			delete capture;
			capture = NULL;
			
			EndDialog(hwnd, return_code);
			return TRUE;
		}
		
		case WM_BEGIN: {
			capture = new wa_capture(replay_path, config);
			capture_path = capture->capture_path;
			
			return TRUE;
		}
		
		case WM_WAEXIT: {
			if(encoders[video_format].type == encoder_info::ffmpeg) {
				log_push("Starting encoder...\r\n");
				
				std::string cmdline = ffmpeg_cmdline(encoders[video_format], capture_path);
				encoder = new exe_launcher(cmdline.c_str(), hwnd, WM_ENC_EXIT);
			}else{
				PostMessage(hwnd, WM_ENC_EXIT, 0, 0);
			}
			
			return TRUE;
		}
		
		case WM_ENC_EXIT: {
			if(do_cleanup) {
				log_push("Cleaning up...\r\n");
				
				delete_frames(capture_path);
				
				DeleteFile(std::string(capture_path + "\\" + FRAME_PREFIX + "pass1.wav").c_str());
				DeleteFile(std::string(capture_path + "\\" + FRAME_PREFIX + "pass2.wav").c_str());
				DeleteFile(std::string(capture_path + "\\" + FRAME_PREFIX + "audio.wav").c_str());
				
				RemoveDirectory(capture_path.c_str());
			}
			
			log_push("Complete!\r\n");
			EnableWindow(GetDlgItem(hwnd, IDOK), TRUE);
			
			return TRUE;
		}
		
		case WM_COMMAND: {
			if(HIWORD(wp) == BN_CLICKED && LOWORD(wp) == IDOK) {
				return_code = 1;
				PostMessage(hwnd, WM_CLOSE, 0, 0);
			}
			
			return TRUE;
		}
		
		case WM_PUSHLOG: {
			HWND log = GetDlgItem(hwnd, LOG_EDIT);
			
			std::string buf = get_window_string(log) + *((std::string*)wp);
			
			SetWindowText(log, buf.c_str());
			
			return TRUE;
		}
		
		default:
			break;
	}
	
	return FALSE;
}

#define UINT_IN(opt_var, window_id, opt_name) \
	if((tmp.opt_var = get_window_uint(GetDlgItem(hwnd, window_id))) == -1) { \
		MessageBox(hwnd, opt_name " must be an integer", NULL, MB_OK | MB_ICONERROR); \
		break; \
	}

#define DOUBLE_IN(opt_var, window_id, opt_name, min, max) \
	if((tmp.opt_var = get_window_double(GetDlgItem(hwnd, window_id))) == -1 || tmp.opt_var < min || tmp.opt_var > max) { \
		MessageBox(hwnd, std::string(std::string(opt_name) + " must be a number between " + to_string(min) + " and " + to_string(max)).c_str(), NULL, MB_OK | MB_ICONERROR); \
		break; \
	}

INT_PTR CALLBACK options_dproc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
	switch(msg) {
		case WM_INITDIALOG: {
			SetWindowText(GetDlgItem(hwnd, MAX_SKEW), to_string(config.max_skew).c_str());
			SetWindowText(GetDlgItem(hwnd, AUDIO_BUF_TIME), to_string(config.audio_buf_time).c_str());
			SetWindowText(GetDlgItem(hwnd, AUDIO_BUF_COUNT), to_string(config.audio_buf_count).c_str());
			
			HWND list = GetDlgItem(hwnd, AUDIO_RATE);
			set_combo_height(list);
			
			ComboBox_AddString(list, "11.025 kHz");
			ComboBox_AddString(list, "22.05 kHz");
			ComboBox_AddString(list, "44.1 kHz");
			ComboBox_AddString(list, "96 kHz");
			
			switch(config.audio_rate) {
				case 11025:
					ComboBox_SetCurSel(list, 0);
					break;
					
				case 22050:
					ComboBox_SetCurSel(list, 1);
					break;
					
				case 44100:
					ComboBox_SetCurSel(list, 2);
					break;
				
				case 96000:
					ComboBox_SetCurSel(list, 3);
					break;
			};
			
			list = GetDlgItem(hwnd, AUDIO_CHANNELS);
			set_combo_height(list);
			
			ComboBox_AddString(list, "Mono");
			ComboBox_AddString(list, "Stereo");
			ComboBox_SetCurSel(list, config.audio_channels - 1);
			
			list = GetDlgItem(hwnd, AUDIO_WIDTH);
			set_combo_height(list);
			
			ComboBox_AddString(list, "8-bit");
			ComboBox_AddString(list, "16-bit");
			ComboBox_SetCurSel(list, config.audio_bits / 8 - 1);
			
			SetWindowText(GetDlgItem(hwnd, SP_BUFFER_LEN), to_string(config.sp_buffer).c_str());
			SetWindowText(GetDlgItem(hwnd, SP_MEAN_FRAMES), to_string(config.sp_mean_frames).c_str());
			SetWindowText(GetDlgItem(hwnd, SP_CMP_FRAMES), to_string(config.sp_cmp_frames).c_str());
			
			Button_SetCheck(GetDlgItem(hwnd, SP_USE_DZ), config.sp_use_dz ? BST_CHECKED : BST_UNCHECKED);
			Button_SetCheck(GetDlgItem(hwnd, SP_DYNAMIC_DZ), config.sp_dynamic_dz ? BST_CHECKED : BST_UNCHECKED);
			SetWindowText(GetDlgItem(hwnd, SP_STATIC_DZ), to_string(config.sp_static_dz).c_str());
			SetWindowText(GetDlgItem(hwnd, SP_DZ_MARGIN), to_string(config.sp_dz_margin).c_str());
			
			SetWindowText(GetDlgItem(hwnd, MAX_ENC_THREADS), to_string(config.max_enc_threads).c_str());
			
			return TRUE;
		}
		
		case WM_COMMAND: {
			if(HIWORD(wp) == BN_CLICKED) {
				if(LOWORD(wp) == IDOK) {
					arec_config tmp = config;
					
					UINT_IN(max_skew, MAX_SKEW, "Max skew");
					UINT_IN(audio_buf_time, AUDIO_BUF_TIME, "Buffer time");
					UINT_IN(audio_buf_count, AUDIO_BUF_COUNT, "Buffer count");
					
					const unsigned int audio_rates[] = {11025, 22050, 44100, 96000};
					
					tmp.audio_rate = audio_rates[ComboBox_GetCurSel(GetDlgItem(hwnd, AUDIO_RATE))];
					
					tmp.audio_channels = ComboBox_GetCurSel(GetDlgItem(hwnd, AUDIO_CHANNELS)) + 1;
					tmp.audio_bits = (ComboBox_GetCurSel(GetDlgItem(hwnd, AUDIO_WIDTH)) + 1) * 8;
					
					UINT_IN(sp_buffer, SP_BUFFER_LEN, "Analysis buffer");
					UINT_IN(sp_mean_frames, SP_MEAN_FRAMES, "Average length");
					UINT_IN(sp_cmp_frames, SP_CMP_FRAMES, "Comparison length");
					
					if(tmp.sp_cmp_frames % tmp.sp_mean_frames) {
						MessageBox(hwnd, "Comparison length must be a multiple of Average length", NULL, MB_OK | MB_ICONERROR);
						break;
					}
					
					tmp.sp_use_dz = (Button_GetCheck(GetDlgItem(hwnd, SP_USE_DZ)) == BST_CHECKED);
					tmp.sp_dynamic_dz = (Button_GetCheck(GetDlgItem(hwnd, SP_DYNAMIC_DZ)) == BST_CHECKED);
					
					DOUBLE_IN(sp_static_dz, SP_STATIC_DZ, "Static dead zone", 0.0, 1.0);
					DOUBLE_IN(sp_dz_margin, SP_DZ_MARGIN, "Dynamic dead zone margin", 0.0, 1.0);
					
					UINT_IN(max_enc_threads, MAX_ENC_THREADS, "Max threads");
					
					config = tmp;
					
					EndDialog(hwnd, 1);
				}else if(LOWORD(wp) == IDCANCEL) {
					PostMessage(hwnd, WM_CLOSE, 0, 0);
				}
			}
			
			return TRUE;
		}
		
		case WM_CLOSE: {
			EndDialog(hwnd, 0);
			return TRUE;
		}
		
		default: {
			break;
		}
	}
	
	return FALSE;
}

int main(int argc, char **argv) {
	InitCommonControls();
	
	audio_sources = get_audio_sources();
	
	reg_handle reg(HKEY_CURRENT_USER, "Software\\Armageddon Recorder", KEY_QUERY_VALUE | KEY_SET_VALUE, true);
	
	wa_path = reg.get_string("wa_path");
	if(wa_path.empty()) {
		reg_handle wa_reg(HKEY_CURRENT_USER, "Software\\Team17SoftwareLTD\\WormsArmageddon", KEY_QUERY_VALUE, false);
		wa_path = wa_reg.get_string("PATH");
		
		if(wa_path.empty()) {
			wa_path = choose_dir(NULL, "Select Worms Armageddon directory:", "wa.exe");
		}
		
		if(wa_path.empty()) {
			MessageBox(NULL, "Worms Armageddon must be installed.", NULL, MB_OK | MB_ICONERROR);
			return 1;
		}
	}
	
	config.load_wormkit_dlls = reg.get_dword("load_wormkit_dlls", false);
	check_wormkit();
	
	std::string fmt = reg.get_string("selected_encoder", "Uncompressed AVI");
	
	load_encoders();
	
	for(unsigned int i = 0; i < encoders.size(); i++) {
		if(fmt == encoders[i].name) {
			video_format = i;
			break;
		}
	}
	
	fmt = reg.get_string("audio_format");
	
	for(unsigned int i = 0; audio_encoders[i].name; i++) {
		if(fmt == std::string(audio_encoders[i].name) || i == 0) {
			audio_format = i;
		}
	}
	
	config.width = reg.get_dword("res_x", 640);
	config.height = reg.get_dword("res_y", 480);
	
	config.frame_rate = reg.get_dword("frame_rate", 50);
	
	config.enable_audio = reg.get_dword("enable_audio", true);
	config.audio_source = reg.get_dword("audio_source", 0);
	config.do_second_pass = reg.get_dword("do_second_pass", false);
	
	config.audio_rate = reg.get_dword("audio_sample_rate", 44100);
	config.audio_bits = reg.get_dword("audio_sample_width", 16);
	config.audio_channels = reg.get_dword("audio_channels", 2);
	
	config.audio_buf_time = reg.get_dword("audio_buf_time", 2);
	config.audio_buf_count = reg.get_dword("audio_buf_count", 64);
	config.max_skew = reg.get_dword("max_skew", 5);
	
	config.sp_buffer = reg.get_dword("sp_buffer", 30);
	config.sp_mean_frames = reg.get_dword("sp_mean_frames", 1);
	config.sp_cmp_frames = reg.get_dword("sp_cmp_frames", 400);
	
	config.sp_use_dz = reg.get_dword("sp_use_dz", true);
	config.sp_dynamic_dz = reg.get_dword("sp_dynamic_dz", true);
	config.sp_static_dz = reg.get_double("sp_static_dz", 0.12);
	config.sp_dz_margin = reg.get_double("sp_dz_margin", 0.5);
	
	config.max_enc_threads = reg.get_dword("max_enc_threads", 0);
	
	config.wa_detail_level = reg.get_dword("wa_detail_level", 0);
	config.wa_chat_behaviour = reg.get_dword("wa_chat_behaviour", 0);
	config.wa_lock_camera = reg.get_dword("wa_lock_camera", true);
	config.wa_bigger_fonts = reg.get_dword("wa_bigger_fonts", true);
	config.wa_transparent_labels = reg.get_dword("wa_transparent_labels", false);
	
	do_cleanup = reg.get_dword("do_cleanup", true);
	
	config.replay_dir = reg.get_string("replay_dir");
	config.video_dir = reg.get_string("video_dir");
	
	while(DialogBox(GetModuleHandle(NULL), MAKEINTRESOURCE(DLG_MAIN), NULL, &main_dproc)) {
		reg.set_string("selected_encoder", encoders[video_format].name);
		reg.set_string("audio_format", audio_encoders[audio_format].name);
		
		reg.set_dword("res_x", config.width);
		reg.set_dword("res_y", config.height);
		
		reg.set_dword("frame_rate", config.frame_rate);
		
		reg.set_dword("enable_audio", config.enable_audio);
		reg.set_dword("audio_source", config.audio_source);
		reg.set_dword("do_second_pass", config.do_second_pass);
		
		reg.set_dword("audio_sample_rate", config.audio_rate);
		reg.set_dword("audio_sample_width", config.audio_bits);
		reg.set_dword("audio_channels", config.audio_channels);
		
		reg.set_dword("audio_buf_time", config.audio_buf_time);
		reg.set_dword("audio_buf_count", config.audio_buf_count);
		reg.set_dword("max_skew", config.max_skew);
		
		reg.set_dword("sp_buffer", config.sp_buffer);
		reg.set_dword("sp_mean_frames", config.sp_mean_frames);
		reg.set_dword("sp_cmp_frames", config.sp_cmp_frames);
		
		reg.set_dword("sp_use_dz", config.sp_use_dz);
		reg.set_dword("sp_dynamic_dz", config.sp_dynamic_dz);
		reg.set_double("sp_static_dz", config.sp_static_dz);
		reg.set_double("sp_dz_margin", config.sp_dz_margin);
		
		reg.set_dword("max_enc_threads", config.max_enc_threads);
		
		reg.set_dword("wa_detail_level", config.wa_detail_level);
		reg.set_dword("wa_chat_behaviour", config.wa_chat_behaviour);
		reg.set_dword("wa_lock_camera", config.wa_lock_camera);
		reg.set_dword("wa_bigger_fonts", config.wa_bigger_fonts);
		reg.set_dword("wa_transparent_labels", config.wa_transparent_labels);
		
		reg.set_dword("do_cleanup", do_cleanup);
		
		reg.set_string("replay_dir", config.replay_dir);
		reg.set_string("video_dir", config.video_dir);
		
		reg.set_string("wa_path", wa_path);
		reg.set_dword("load_wormkit_dlls", config.load_wormkit_dlls);
		
		if(!DialogBox(GetModuleHandle(NULL), MAKEINTRESOURCE(DLG_PROGRESS), NULL, &prog_dproc)) {
			break;
		}
	}
	
	if(com_init) {
		CoUninitialize();
	}
	
	return 0;
}
