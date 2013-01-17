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

#include <exception>
#include <limits.h>

#define WM_WAEXIT     (WM_USER + 1)
#define WM_PUSHLOG    (WM_USER + 2)
#define WM_BEGIN      (WM_USER + 3)
#define WM_ENC_EXIT   (WM_USER + 4)
#define WM_AUDIO_DONE (WM_USER + 6)
#define WM_ABORTED    (WM_USER + 7)

extern HWND progress_dialog;

class bad_input: public std::exception {};

std::string get_window_string(HWND hwnd);
int get_window_int(HWND window, int min = INT_MIN, int max = INT_MAX);
double get_window_double(HWND window);

bool checkbox_get(HWND hwnd);
void checkbox_set(HWND hwnd, bool checked);

void menu_item_enable(HMENU menu, UINT item, bool enable);
bool menu_item_get(HMENU menu, UINT item);
void menu_item_set(HMENU menu, UINT item, bool state);
bool menu_item_toggle(HMENU menu, UINT item);

void volume_init(HWND slider, HWND edit, int value);
void volume_on_slider(HWND slider, HWND edit);

INT_PTR CALLBACK prog_dproc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);

void log_push(const std::string &msg);
void show_error(const std::string &msg);

#endif /* !AREC_UI_HPP */
