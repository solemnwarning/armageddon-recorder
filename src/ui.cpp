/* Armageddon Recorder - UI code
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
#include <assert.h>

#include "main.hpp"
#include "ui.hpp"
#include "capture.hpp"
#include "resource.h"
#include "encode.hpp"

HWND progress_dialog = NULL;

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

INT_PTR CALLBACK prog_dproc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
	static wa_capture *capture = NULL;
	
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
			ffmpeg_cleanup();
			
			delete capture;
			capture = NULL;
			
			EndDialog(hwnd, return_code);
			return TRUE;
		}
		
		case WM_BEGIN: {
			/* Start capture */
			
			try {
				capture = new wa_capture(config);
			} catch(const arec::error &err) {
				SendMessage(hwnd, WM_CAUGHT, (WPARAM)err.what(), 0);
			}
			
			return TRUE;
		}
		
		case WM_WAEXIT: {
			if(encoders[config.video_format].type == encoder_info::ffmpeg) {
				log_push("Starting encoder...\r\n");
				
				try {
					ffmpeg_run(config);
				} catch(const arec::error &err) {
					SendMessage(hwnd, WM_CAUGHT, (WPARAM)err.what(), 0);
				}
			}else{
				PostMessage(hwnd, WM_ENC_EXIT, 0, 0);
			}
			
			return TRUE;
		}
		
		case WM_ENC_EXIT: {
			/* The encoder process has exited */
			
			if(config.do_cleanup) {
				log_push("Cleaning up...\r\n");
				
				delete_capture(config.capture_dir);
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
			/* Append text to the log window
			 * WPARAM: const std::string* containing text to append
			*/
			
			HWND log = GetDlgItem(hwnd, LOG_EDIT);
			
			std::string buf = get_window_string(log) + *((const std::string*)wp);
			
			SetWindowText(log, buf.c_str());
			
			return TRUE;
		}
		
		case WM_CAUGHT: {
			/* Exception caught, capture/encode/etc aborted
			 * WPARAM: const char* containing error details
			 * LPARAM: Nonzero if WPARAM should be deleted
			*/
			
			ffmpeg_cleanup();
			
			delete capture;
			capture = NULL;
			
			log_push((const char*)wp);
			
			if(lp) {
				delete (char*)wp;
			}
			
			MessageBox(hwnd, "Capture aborted due to error.\nCheck log window for details.", NULL, MB_OK | MB_ICONERROR);
			
			return TRUE;
		}
		
		default:
			break;
	}
	
	return FALSE;
}

/* Append text to the progress dialog log window */
void log_push(const std::string &msg) {
	SendMessage(progress_dialog, WM_PUSHLOG, (WPARAM)&msg, 0);
}
