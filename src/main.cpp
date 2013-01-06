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
#include "ui.hpp"

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

arec_config config;

std::string wa_path;
bool wormkit_exe;

bool com_init = false;		/* COM has been initialized in the main thread */

reg_handle wa_options(HKEY_CURRENT_USER, "Software\\Team17SoftwareLTD\\WormsArmageddon\\Options", KEY_QUERY_VALUE | KEY_SET_VALUE, false);

INT_PTR CALLBACK options_dproc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);

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
		GetFileAttributes(std::string(wa_path + "\\HookLib.dll").c_str()) != INVALID_FILE_ATTRIBUTES
	);
}

static void volume_init(HWND slider, HWND edit, int value);
static void volume_set_edit(HWND slider, HWND edit);

static void volume_init(HWND slider, HWND edit, int value)
{
	SendMessage(slider, TBM_SETRANGE, (WPARAM)(FALSE), MAKELPARAM(0, 100));
	SendMessage(slider, TBM_SETTICFREQ, (WPARAM)(5), (LPARAM)(0));
	
	SendMessage(slider, TBM_SETPOS, (WPARAM)(TRUE), (LPARAM)(value));
	
	volume_set_edit(slider, edit);
}

static void volume_set_edit(HWND slider, HWND edit)
{
	int pos = SendMessage(slider, TBM_GETPOS, (WPARAM)(0), (LPARAM)(0));
	
	SetWindowText(edit, std::string(to_string(pos) + "%").c_str());
}

static void toggle_clipping(HWND hwnd)
{
	bool enabled = checkbox_get(GetDlgItem(hwnd, FIX_CLIPPING));
	
	EnableWindow(GetDlgItem(hwnd, STEP_VOL_SLIDER), enabled);
	EnableWindow(GetDlgItem(hwnd, STEP_VOL_EDIT), enabled);
	
	EnableWindow(GetDlgItem(hwnd, MIN_VOL_SLIDER), enabled);
	EnableWindow(GetDlgItem(hwnd, MIN_VOL_EDIT), enabled);
}

INT_PTR CALLBACK main_dproc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
	switch(msg) {
		case WM_INITDIALOG: {
			SendMessage(hwnd, WM_SETICON, 0, (LPARAM)LoadIcon(GetModuleHandle(NULL), MAKEINTRESOURCE(ICON16)));
			SendMessage(hwnd, WM_SETICON, 1, (LPARAM)LoadIcon(GetModuleHandle(NULL), MAKEINTRESOURCE(ICON32)));
			
			EnableMenuItem(GetMenu(hwnd), LOAD_WORMKIT_DLLS, wormkit_exe ? MF_ENABLED : MF_GRAYED);
			CheckMenuItem(GetMenu(hwnd), LOAD_WORMKIT_DLLS, (wormkit_exe && config.load_wormkit_dlls) ? MF_CHECKED : MF_UNCHECKED);
			
			SetWindowText(GetDlgItem(hwnd, RES_X), to_string(config.width).c_str());
			SetWindowText(GetDlgItem(hwnd, RES_Y), to_string(config.height).c_str());
			
			HWND fmt_list = GetDlgItem(hwnd, VIDEO_FORMAT);
			
			for(unsigned int i = 0; i < encoders.size(); i++) {
				ComboBox_AddString(fmt_list, encoders[i].name.c_str());
			}
			
			ComboBox_SetCurSel(fmt_list, config.video_format);
			set_combo_height(fmt_list);
			
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
			
			ComboBox_SetCurSel(audio_fmt_list, config.audio_format);
			set_combo_height(audio_fmt_list);
			
			Button_SetCheck(GetDlgItem(hwnd, DO_CLEANUP), (config.do_cleanup ? BST_CHECKED : BST_UNCHECKED));
			
			volume_init(GetDlgItem(hwnd, INIT_VOL_SLIDER), GetDlgItem(hwnd, INIT_VOL_EDIT), config.init_vol);
			
			checkbox_set(GetDlgItem(hwnd, FIX_CLIPPING), true);
			toggle_clipping(hwnd);
			
			volume_init(GetDlgItem(hwnd, STEP_VOL_SLIDER), GetDlgItem(hwnd, STEP_VOL_EDIT), config.step_vol);
			volume_init(GetDlgItem(hwnd, MIN_VOL_SLIDER), GetDlgItem(hwnd, MIN_VOL_EDIT), config.min_vol);
			
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
						config.replay_file = get_window_string(GetDlgItem(hwnd, REPLAY_PATH));
						config.video_file = get_window_string(GetDlgItem(hwnd, AVI_PATH));
						config.video_format = ComboBox_GetCurSel(GetDlgItem(hwnd, VIDEO_FORMAT));
						config.audio_format = ComboBox_GetCurSel(GetDlgItem(hwnd, AUDIO_FORMAT_MENU));
						
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
						
						config.do_cleanup = Button_GetCheck(GetDlgItem(hwnd, DO_CLEANUP));
						
						/* Audio settings */
						
						config.init_vol = SendMessage(GetDlgItem(hwnd, INIT_VOL_SLIDER), TBM_GETPOS, (WPARAM)(0), (LPARAM)(0));
						
						config.fix_clipping = checkbox_get(GetDlgItem(hwnd, FIX_CLIPPING));
						
						config.step_vol = SendMessage(GetDlgItem(hwnd, STEP_VOL_SLIDER), TBM_GETPOS, (WPARAM)(0), (LPARAM)(0));
						config.min_vol  = SendMessage(GetDlgItem(hwnd, MIN_VOL_SLIDER), TBM_GETPOS, (WPARAM)(0), (LPARAM)(0));
						
						if(config.video_format == 0 && config.do_cleanup) {
							MessageBox(hwnd, "You've chosen to not create a video file and delete frames/audio when finished. You probably don't want this.", NULL, MB_OK | MB_ICONWARNING);
							break;
						}
						
						if(config.video_format) {
							if(config.video_file.empty()) {
								MessageBox(hwnd, "Output video filename is required", NULL, MB_OK | MB_ICONERROR);
								break;
							}
						}
						
						/* Fill in convenience variables */
						
						config.replay_name = config.replay_file;
						
						size_t last_slash = config.replay_name.find_last_of('\\');
						if(last_slash != std::string::npos) {
							config.replay_name.erase(0, last_slash + 1);
						}
						
						size_t last_dot = config.replay_name.find_last_of('.');
						if(last_dot != std::string::npos) {
							config.replay_name.erase(last_dot);
						}
						
						config.capture_dir = wa_path + "\\User\\Capture\\" + config.replay_name;
						
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
							config.replay_file = filename;
							SetWindowText(GetDlgItem(hwnd, REPLAY_PATH), config.replay_file.c_str());
							
							config.replay_dir = config.replay_file;
							config.replay_dir.erase(config.replay_dir.find_last_of('\\'));
						}else if(CommDlgExtendedError()) {
							MessageBox(hwnd, std::string("GetOpenFileName: " + to_string(CommDlgExtendedError())).c_str(), NULL, MB_OK | MB_ICONERROR);
						}
						
						break;
					}
					
					case FIX_CLIPPING:
					{
						toggle_clipping(hwnd);
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
						openfile.lpstrDefExt = encoders[config.video_format].default_ext;
						
						if(GetSaveFileName(&openfile)) {
							config.video_file = filename;
							SetWindowText(GetDlgItem(hwnd, AVI_PATH), config.video_file.c_str());
							
							config.video_dir = config.video_file;
							config.video_dir.erase(config.video_dir.find_last_of('\\'));
						}else if(CommDlgExtendedError()) {
							MessageBox(hwnd, std::string("GetSaveFileName: " + to_string(CommDlgExtendedError())).c_str(), NULL, MB_OK | MB_ICONERROR);
						}
						
						break;
					}
					
					case SELECT_WA_DIR: {
						std::string dir = choose_dir(hwnd, "Select Worms Armageddon directory:", "wa.exe");
						if(!dir.empty())
						{
							wa_path = dir;
							
							check_wormkit();
							
							EnableMenuItem(GetMenu(hwnd), LOAD_WORMKIT_DLLS, wormkit_exe ? MF_ENABLED : MF_GRAYED);
							CheckMenuItem(GetMenu(hwnd), LOAD_WORMKIT_DLLS, (wormkit_exe && config.load_wormkit_dlls) ? MF_CHECKED : MF_UNCHECKED);
							
							init_wav_search_path();
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
					
					config.video_format = ComboBox_GetCurSel(GetDlgItem(hwnd, VIDEO_FORMAT));
					
					EnableWindow(GetDlgItem(hwnd, AVI_PATH), config.video_format != 0);
					EnableWindow(GetDlgItem(hwnd, AVI_BROWSE), config.video_format != 0);
					
					return TRUE;
				}
			}
		}
		
		case WM_HSCROLL:
		{
			if((HWND)(lp) == NULL)
			{
				break;
			}
			
			int scroll_id = GetWindowLong((HWND)(lp), GWL_ID);
			
			switch(scroll_id)
			{
				case INIT_VOL_SLIDER:
				{
					volume_set_edit((HWND)(lp), GetDlgItem(hwnd, INIT_VOL_EDIT));
					break;
				}
				
				case STEP_VOL_SLIDER:
				{
					volume_set_edit((HWND)(lp), GetDlgItem(hwnd, STEP_VOL_EDIT));
					break;
				}
				
				case MIN_VOL_SLIDER:
				{
					volume_set_edit((HWND)(lp), GetDlgItem(hwnd, MIN_VOL_EDIT));
					break;
				}
			}
			
			break;
		}
		
		default:
			break;
	}
	
	return FALSE;
}

std::string escape_filename(std::string name) {
	for(size_t i = 0; i < name.length(); i++) {
		if(name[i] == '\\') {
			name.insert(i++, "\\");
		}
	}
	
	return name;
}

#define UINT_IN(opt_var, window_id, opt_name) \
	if((tmp.opt_var = get_window_uint(GetDlgItem(hwnd, window_id))) == -1) { \
		MessageBox(hwnd, opt_name " must be an integer", NULL, MB_OK | MB_ICONERROR); \
		break; \
	}

INT_PTR CALLBACK options_dproc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
	switch(msg) {
		case WM_INITDIALOG: {
			SetWindowText(GetDlgItem(hwnd, MAX_ENC_THREADS), to_string(config.max_enc_threads).c_str());
			
			return TRUE;
		}
		
		case WM_COMMAND: {
			if(HIWORD(wp) == BN_CLICKED) {
				if(LOWORD(wp) == IDOK) {
					arec_config tmp = config;
					
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

int main(int argc, char **argv)
{
	SetErrorMode(SEM_FAILCRITICALERRORS);
	
	gc_initialize(NULL);
	
	InitCommonControls();
	
	reg_handle reg(HKEY_CURRENT_USER, "Software\\Armageddon Recorder", KEY_QUERY_VALUE | KEY_SET_VALUE, true);
	
	wa_path = reg.get_string("wa_path");
	if(wa_path.empty())
	{
		reg_handle wa_reg(HKEY_CURRENT_USER, "Software\\Team17SoftwareLTD\\WormsArmageddon", KEY_QUERY_VALUE, false);
		wa_path = wa_reg.get_string("PATH");
		
		if(wa_path.empty())
		{
			wa_path = choose_dir(NULL, "Select Worms Armageddon directory:", "wa.exe");
		}
		
		if(wa_path.empty())
		{
			MessageBox(NULL, "Worms Armageddon must be installed.", NULL, MB_OK | MB_ICONERROR);
			return 1;
		}
	}
	
	config.load_wormkit_dlls = reg.get_dword("load_wormkit_dlls", false);
	check_wormkit();
	
	init_wav_search_path();
	
	std::string fmt = reg.get_string("selected_encoder", "Uncompressed AVI");
	
	load_encoders();
	
	for(unsigned int i = 0; i < encoders.size(); i++) {
		if(fmt == encoders[i].name) {
			config.video_format = i;
			break;
		}
	}
	
	fmt = reg.get_string("audio_format");
	
	for(unsigned int i = 0; audio_encoders[i].name; i++) {
		if(fmt == std::string(audio_encoders[i].name) || i == 0) {
			config.audio_format = i;
		}
	}
	
	config.width = reg.get_dword("res_x", 640);
	config.height = reg.get_dword("res_y", 480);
	
	config.frame_rate = reg.get_dword("frame_rate", 50);
	
	config.max_enc_threads = reg.get_dword("max_enc_threads", 0);
	
	config.wa_detail_level = reg.get_dword("wa_detail_level", 0);
	config.wa_chat_behaviour = reg.get_dword("wa_chat_behaviour", 0);
	config.wa_lock_camera = reg.get_dword("wa_lock_camera", true);
	config.wa_bigger_fonts = reg.get_dword("wa_bigger_fonts", true);
	config.wa_transparent_labels = reg.get_dword("wa_transparent_labels", false);
	
	config.do_cleanup = reg.get_dword("do_cleanup", true);
	
	config.init_vol     = reg.get_dword("init_vol", 100);
	
	config.fix_clipping = reg.get_dword("fix_clipping", true);
	config.step_vol     = reg.get_dword("step_vol", 5);
	config.min_vol      = reg.get_dword("min_vol", 40);
	
	config.replay_dir = reg.get_string("replay_dir");
	config.video_dir = reg.get_string("video_dir");
	
	while(DialogBox(GetModuleHandle(NULL), MAKEINTRESOURCE(DLG_MAIN), NULL, &main_dproc)) {
		reg.set_string("selected_encoder", encoders[config.video_format].name);
		reg.set_string("audio_format", audio_encoders[config.audio_format].name);
		
		reg.set_dword("res_x", config.width);
		reg.set_dword("res_y", config.height);
		
		reg.set_dword("frame_rate", config.frame_rate);
		
		reg.set_dword("max_enc_threads", config.max_enc_threads);
		
		reg.set_dword("wa_detail_level", config.wa_detail_level);
		reg.set_dword("wa_chat_behaviour", config.wa_chat_behaviour);
		reg.set_dword("wa_lock_camera", config.wa_lock_camera);
		reg.set_dword("wa_bigger_fonts", config.wa_bigger_fonts);
		reg.set_dword("wa_transparent_labels", config.wa_transparent_labels);
		
		reg.set_dword("do_cleanup", config.do_cleanup);
		
		reg.set_dword("init_vol", config.init_vol);
		
		reg.set_dword("fix_clipping", config.fix_clipping);
		reg.set_dword("step_vol", config.step_vol);
		reg.set_dword("min_vol", config.min_vol);
		
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
	
	/* Unload any wav files before gc_shutdown as they each have a gc_Sound
	 * reference.
	*/
	
	wav_files.clear();
	
	gc_shutdown();
	
	return 0;
}

/* Convert a windows error number to an error message */
const char *w32_error(DWORD errnum) {
	static char buf[1024] = {'\0'};
	
	FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM, NULL, errnum, 0, buf, 1023, NULL);
	buf[strcspn(buf, "\r\n")] = '\0';
	return buf;	
}
