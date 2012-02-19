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

#ifndef AREC_REG_HPP
#define AREC_REG_HPP

#include <windows.h>
#include <string>

struct reg_handle {
	HKEY handle;
	
	reg_handle(HKEY key, const char *subkey, REGSAM access, bool create);
	~reg_handle();
	
	DWORD get_dword(const char *name, DWORD default_val = 0);
	std::string get_string(const char *name, const std::string &default_val = std::string());
	double get_double(const char *name, double default_val = 0);
	
	void set_dword(const char *name, DWORD value);
	void set_string(const char *name, const std::string &value);
	void set_double(const char *name, double value);
};

#endif /* !AREC_REG_HPP */
