/* Armageddon Recorder - Registry functions
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
#include <string>

#include "reg.hpp"

reg_handle::reg_handle(HKEY key, const char *subkey, REGSAM access, bool create) {
	if(create) {
		if(RegCreateKeyEx(key, subkey, 0, NULL, 0, access, NULL, &handle, NULL) != ERROR_SUCCESS) {
			handle = NULL;
		}
	}else{
		if(RegOpenKeyEx(key, subkey, 0, access, &handle) != ERROR_SUCCESS) {
			handle = NULL;
		}
	}
}

reg_handle::~reg_handle() {
	if(handle) {
		RegCloseKey(handle);
		handle = NULL;
	}
}

DWORD reg_handle::get_dword(const char *name, DWORD default_val) {
	if(handle) {
		DWORD buf, size = sizeof(DWORD);
		
		if(RegQueryValueEx(handle, name, NULL, NULL, (BYTE*)&buf, &size) != ERROR_SUCCESS) {
			return default_val;
		}
		
		return buf;
	}
	
	return default_val;
}

std::string reg_handle::get_string(const char *name, const std::string &default_val) {
	if(handle) {
		DWORD size = 0;
		
		if(RegQueryValueEx(handle, name, NULL, NULL, NULL, &size) != ERROR_SUCCESS) {
			return default_val;
		}
		
		char *buf = new char[size + 1];
		
		if(RegQueryValueEx(handle, name, NULL, NULL, (BYTE*)buf, &size) != ERROR_SUCCESS) {
			delete buf;
			return default_val;
		}
		
		buf[size] = '\0';
		
		std::string value = buf;
		delete buf;
		
		return value;
	}
	
	return default_val;
}

void reg_handle::set_dword(const char *name, DWORD value) {
	if(handle) {
		RegSetValueEx(handle, name, 0, REG_DWORD, (BYTE*)&value, sizeof(value));
	}
}

void reg_handle::set_string(const char *name, const std::string &value) {
	if(handle) {
		RegSetValueEx(handle, name, 0, REG_SZ, (BYTE*)value.c_str(), value.length() + 1);
	}
}
