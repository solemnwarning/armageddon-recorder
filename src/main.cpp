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
#include <sstream>
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

/* I thought we were past missing things, MinGW... */
#define BIF_NONEWFOLDERBUTTON 0x00000200
typedef LPITEMIDLIST PIDLIST_ABSOLUTE;

std::string replay_path;
std::string start_time, end_time;
unsigned int res_x, res_y;
unsigned int frame_rate;

bool enable_audio;
unsigned int audio_source;

std::string video_path;
unsigned int video_format = 2;

bool show_background;
bool do_cleanup;

std::string wa_path;

HWND progress_dialog = NULL;
bool com_init = false;		/* COM has been initialized in the main thread */

reg_handle wa_options(HKEY_CURRENT_USER, "Software\\Team17SoftwareLTD\\WormsArmageddon\\Options", KEY_QUERY_VALUE | KEY_SET_VALUE, false);

std::string get_window_string(HWND hwnd) {
	char buf[1024];
	
	GetWindowText(hwnd, buf, sizeof(buf));
	return buf;
}

template<class T> std::string to_string(const T& in) {
	std::stringstream os;
	os << in;
	
	return os.str();
};

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

INT_PTR CALLBACK main_dproc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
	switch(msg) {
		case WM_INITDIALOG: {
			SendMessage(hwnd, WM_SETICON, 0, (LPARAM)LoadIcon(GetModuleHandle(NULL), MAKEINTRESOURCE(ICON16)));
			SendMessage(hwnd, WM_SETICON, 1, (LPARAM)LoadIcon(GetModuleHandle(NULL), MAKEINTRESOURCE(ICON32)));
			
			SetWindowText(GetDlgItem(hwnd, RES_X), to_string(res_x).c_str());
			SetWindowText(GetDlgItem(hwnd, RES_Y), to_string(res_y).c_str());
			
			HWND fmt_list = GetDlgItem(hwnd, VIDEO_FORMAT);
			
			for(unsigned int i = 0; i < encoders.size(); i++) {
				ComboBox_AddString(fmt_list, encoders[i].name.c_str());
				
				if(i == video_format) {
					ComboBox_SetCurSel(fmt_list, i);
				}
			}
			
			HWND audio_list = GetDlgItem(hwnd, AUDIO_SOURCE);
			
			ComboBox_AddString(audio_list, "None");
			ComboBox_SetCurSel(audio_list, 0);
			
			std::vector<WAVEINCAPS> sources = get_audio_sources();
			
			for(unsigned int i = 0; i < sources.size(); i++) {
				ComboBox_AddString(audio_list, sources[i].szPname);
				
				if(enable_audio && (i == audio_source || i == 0)) {
					ComboBox_SetCurSel(audio_list, i + 1);
				}
			}
			
			SetWindowText(GetDlgItem(hwnd, FRAMES_SEC), to_string(frame_rate).c_str());
			
			Button_SetCheck(GetDlgItem(hwnd, SHOW_BACKGROUND), (show_background ? BST_CHECKED : BST_UNCHECKED));
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
						
						audio_source = ComboBox_GetCurSel(GetDlgItem(hwnd, AUDIO_SOURCE));
						enable_audio = (audio_source-- > 0);
						
						std::string rx_string = get_window_string(GetDlgItem(hwnd, RES_X));
						std::string ry_string = get_window_string(GetDlgItem(hwnd, RES_Y));
						
						if(!validate_res(rx_string) || !validate_res(ry_string)) {
							MessageBox(hwnd, "Invalid resolution", NULL, MB_OK | MB_ICONERROR);
							break;
						}
						
						res_x = strtoul(rx_string.c_str(), NULL, 10);
						res_y = strtoul(ry_string.c_str(), NULL, 10);
						
						std::string fps_text = get_window_string(GetDlgItem(hwnd, FRAMES_SEC));
						
						if(!validate_fps(fps_text)) {
							MessageBox(hwnd, "Frame rate must be an integer in the range 1-50", NULL, MB_OK | MB_ICONERROR);
							break;
						}
						
						frame_rate = atoi(fps_text.c_str());
						
						start_time = get_window_string(GetDlgItem(hwnd, TIME_START));
						end_time = get_window_string(GetDlgItem(hwnd, TIME_END));
						
						if(!validate_time(start_time)) {
							MessageBox(hwnd, "Invalid start time", NULL, MB_OK | MB_ICONERROR);
							break;
						}
						
						if(!validate_time(end_time)) {
							MessageBox(hwnd, "Invalid end time", NULL, MB_OK | MB_ICONERROR);
							break;
						}
						
						show_background = Button_GetCheck(GetDlgItem(hwnd, SHOW_BACKGROUND));
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
						openfile.lpstrTitle = "Select replay";
						openfile.Flags = OFN_FILEMUSTEXIST | OFN_HIDEREADONLY;
						
						if(GetOpenFileName(&openfile)) {
							replay_path = filename;
							SetWindowText(GetDlgItem(hwnd, REPLAY_PATH), replay_path.c_str());
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
						openfile.lpstrTitle = "Save video as...";
						openfile.Flags = OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT;
						openfile.lpstrDefExt = encoders[video_format].default_ext;
						
						if(GetSaveFileName(&openfile)) {
							video_path = filename;
							SetWindowText(GetDlgItem(hwnd, AVI_PATH), video_path.c_str());
						}else if(CommDlgExtendedError()) {
							MessageBox(hwnd, std::string("GetSaveFileName: " + to_string(CommDlgExtendedError())).c_str(), NULL, MB_OK | MB_ICONERROR);
						}
						
						break;
					}
					
					case SELECT_WA_DIR: {
						std::string dir = choose_dir(hwnd, "Select Worms Armageddon directory:", "wa.exe");
						if(!dir.empty()) {
							wa_path = dir;
						}
						
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
	
	std::string cmdline = "ffmpeg.exe -threads 0 -y -r " + to_string(frame_rate) + " -i \"" + frames_in + "\"";
	
	if(enable_audio) {
		cmdline.append(std::string(" -i \"") + audio_in + "\"");
	}
	
	cmdline.append(std::string(" -vcodec ") + format.video_format + " -acodec " + format.audio_format);
	
	if(format.name == "H.264 (Lossless)") {
		cmdline.append(" -qmin 0 -qmax 0");
	}
	
	if(format.bps_pix) {
		unsigned int bps = res_x * res_y;
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
	
	static DWORD orig_detail = 0;
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
			orig_detail = wa_options.get_dword("DetailLevel", 0);
			wa_options.set_dword("DetailLevel", show_background ? 5 : 0);
			
			capture = new wa_capture(replay_path, res_x, res_y, frame_rate, start_time, end_time);
			capture_path = capture->capture_path;
			
			return TRUE;
		}
		
		case WM_WAEXIT: {
			wa_options.set_dword("DetailLevel", orig_detail);
			
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

int main(int argc, char **argv) {
	InitCommonControls();
	
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
	
	std::string fmt = reg.get_string("selected_encoder", "Uncompressed AVI");
	
	load_encoders();
	
	for(unsigned int i = 0; i < encoders.size(); i++) {
		if(fmt == encoders[i].name) {
			video_format = i;
			break;
		}
	}
	
	res_x = reg.get_dword("res_x", 640);
	res_y = reg.get_dword("res_y", 480);
	
	frame_rate = reg.get_dword("frame_rate", 25);
	
	enable_audio = reg.get_dword("enable_audio", true);
	audio_source = reg.get_dword("audio_source", 0);
	
	show_background = reg.get_dword("show_background", false);
	do_cleanup = reg.get_dword("do_cleanup", true);
	
	while(DialogBox(GetModuleHandle(NULL), MAKEINTRESOURCE(DLG_MAIN), NULL, &main_dproc)) {
		reg.set_string("selected_encoder", encoders[video_format].name);
		
		reg.set_dword("res_x", res_x);
		reg.set_dword("res_y", res_y);
		
		reg.set_dword("frame_rate", frame_rate);
		
		reg.set_dword("enable_audio", enable_audio);
		reg.set_dword("audio_source", audio_source);
		
		reg.set_dword("show_background", show_background);
		reg.set_dword("do_cleanup", do_cleanup);
		
		reg.set_string("wa_path", wa_path);
		
		if(!DialogBox(GetModuleHandle(NULL), MAKEINTRESOURCE(DLG_PROGRESS), NULL, &prog_dproc)) {
			break;
		}
	}
	
	if(com_init) {
		CoUninitialize();
	}
	
	return 0;
}
