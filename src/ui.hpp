/* Armageddon Recorder - UI header
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

#ifndef AREC_UI_HPP
#define AREC_UI_HPP

#define WM_WAEXIT     (WM_USER + 1)
#define WM_PUSHLOG    (WM_USER + 2)
#define WM_BEGIN      (WM_USER + 3)
#define WM_ENC_EXIT   (WM_USER + 4)
#define WM_AUDIO_DONE (WM_USER + 6)
#define WM_ABORTED    (WM_USER + 7)

extern HWND progress_dialog;

std::string get_window_string(HWND hwnd);
size_t get_window_uint(HWND window);
double get_window_double(HWND window);

INT_PTR CALLBACK prog_dproc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);

void log_push(const std::string &msg);

#endif /* !AREC_UI_HPP */
