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
#include <map>
#include <assert.h>
#include <time.h>
#include <stdio.h>

#include "capture.hpp"
#include "ui.hpp"
#include "audiolog.h"
#include "main.hpp"

static unsigned int frame_count;

static std::map<std::string, DWORD> original_options;

static HANDLE monitor_thread = NULL;
static HANDLE wa_process     = NULL;
static char *wa_cmdline      = NULL;

/* Wait for the WA.exe process to exit and send a WM_WAEXIT message to the
 * progress dialog with the exit code.
*/
static DWORD WINAPI wa_monitor(LPVOID lpParameter)
{
	WaitForSingleObject(wa_process, INFINITE);
	
	DWORD exit_code;
	GetExitCodeProcess(wa_process, &exit_code);
	
	PostMessage(progress_dialog, WM_WAEXIT, (WPARAM)(exit_code), 0);
	
	return 0;
}

/* Returns the number of frames exported by the current capture. */
unsigned int get_frame_count()
{
	while(1)
	{
		char path[1024];
		
		snprintf(path, sizeof(path), "%s\\" FRAME_PREFIX "%06u.png", config.capture_dir.c_str(), frame_count);
		
		if(GetFileAttributes(path) != INVALID_FILE_ATTRIBUTES)
		{
			frame_count++;
		}
		else{
			break;
		}
	}
	
	return frame_count;
}

static void set_option(const char *name, DWORD value, DWORD def_value = 0)
{
	original_options.insert(std::make_pair(name, wa_options.get_dword(name, def_value)));
	wa_options.set_dword(name, value);
}

static void restore_options()
{
	for(std::map<std::string, DWORD>::iterator i = original_options.begin(); i != original_options.end(); i++)
	{
		wa_options.set_dword(i->first.c_str(), i->second);
	}
	
	original_options.clear();
}

bool start_capture()
{
	log_push("Preparing to capture " + config.replay_name + "...\r\n");
	
	frame_count = 0;
	
	/* Delete any previous capture and create an empty capture directory as
	 * some code depends on it being there.
	*/
	
	delete_capture();
	CreateDirectory(config.capture_dir.c_str(), NULL);
	
	/* Override WA options. The originals are restored when the capture is
	 * finished.
	 * 
	 * TODO: Store originals somewhere nonvolatile?
	*/
	
	set_option("DetailLevel",      config.wa_detail_level,        5);
	set_option("DisablePhone",     config.wa_chat_behaviour != 1, 0);
	set_option("ChatPinned",       config.wa_chat_behaviour == 2, 0);
	set_option("HomeLock",         config.wa_lock_camera,         0);
	set_option("LargerFonts",      config.wa_bigger_fonts,        0);
	set_option("InfoTransparency", config.wa_transparent_labels,  0);
	
	/* Build the command line and copy it to a persistent buffer. */
	
	std::string cmdline = "\"" + wa_path + "\\WA.exe\" /getvideo"
		" \"" + config.replay_file + "\""
		" \"" + to_string((double)(50) / config.frame_rate) + "\""
		" \"" + config.start_time + "\" \"" + config.end_time + "\""
		" \"" + to_string(config.width) + "\" \"" + to_string(config.height) + "\""
		" \"" + FRAME_PREFIX + "\"";
	
	wa_cmdline = new char[cmdline.length() + 1];
	strcpy(wa_cmdline, cmdline.c_str());
	
	/* Environment variables used by dsound.dll */
	
	SetEnvironmentVariable("AREC_CAPTURE_DIRECTORY", config.capture_dir.c_str());
	
	if(config.load_wormkit_dlls)
	{
		SetEnvironmentVariable("AREC_LOAD_WORMKIT", "1");
	}
	
	STARTUPINFO sinfo;
	memset(&sinfo, 0, sizeof(sinfo));
	sinfo.cb = sizeof(sinfo);
	
	PROCESS_INFORMATION pinfo;
	
	log_push("Starting WA...\r\n");
	
	if(!CreateProcess(NULL, wa_cmdline, NULL, NULL, FALSE, 0, NULL, NULL, &sinfo, &pinfo))
	{
		log_push(std::string("Could not execute WA.exe: ") + w32_error(GetLastError()) + "\r\n");
		
		finish_capture();
		
		return false;
	}
	
	wa_process = pinfo.hProcess;
	CloseHandle(pinfo.hThread);
	
	assert((monitor_thread = CreateThread(NULL, 0, &wa_monitor, NULL, 0, NULL)));
	
	return true;
}

/* Terminate the monitor thread and WA.exe process, cleanup any resources used
 * by the capture and undo any changes made to the WA installation.
*/
void finish_capture()
{
	/* Make the sure monitor thread and WA have exited before closing
	 * their handles...
	*/
	
	if(monitor_thread)
	{
		TerminateThread(monitor_thread, 1);
		
		CloseHandle(monitor_thread);
		monitor_thread = NULL;
	}
	
	if(wa_process)
	{
		TerminateProcess(wa_process, 1);
		
		CloseHandle(wa_process);
		wa_process = NULL;
	}
	
	delete wa_cmdline;
	wa_cmdline = NULL;
	
	restore_options();
}

void delete_capture()
{
	WIN32_FIND_DATA file;
	
	HANDLE dir = FindFirstFile(std::string(config.capture_dir + "\\" + FRAME_PREFIX + "*").c_str(), &file);
	
	while(dir != INVALID_HANDLE_VALUE)
	{
		DeleteFile(std::string(config.capture_dir + "\\" + file.cFileName).c_str());
		
		if(!FindNextFile(dir, &file))
		{
			FindClose(dir);
			break;
		}
	}
	
	RemoveDirectory(config.capture_dir.c_str());
}
