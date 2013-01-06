/* Armageddon Recorder - Audio functions
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

#define __STDC_LIMIT_MACROS

#include <windows.h>
#include <string>
#include <stdio.h>
#include <assert.h>
#include <gorilla/ga.h>
#include <gorilla/gau.h>
#include <sndfile.h>
#include <MurmurHash3.h>

#include "audio.hpp"
#include "audiolog.h"
#include "ui.hpp"

std::map<uint32_t, wav_file> wav_files;

wav_file::wav_file(const std::string &_path)
{
	path = _path;
	
	assert((sound = gau_load_sound_file(path.c_str(), "wav")));
}

wav_file::wav_file(const wav_file &src)
{
	path  = src.path;
	sound = src.sound;
	
	ga_sound_acquire(sound);
}

wav_file::~wav_file()
{
	ga_sound_release(sound);
}

static std::vector<std::string> wav_search_path;

/* Initialise the wav search path using the WA installation directory and CD-ROM
 * if present.
*/
void init_wav_search_path()
{
	wav_search_path.clear();
	
	wav_search_path.push_back(wa_path + "\\DATA\\Wav\\Effects");
	wav_search_path.push_back(wa_path + "\\FESfx");
	wav_search_path.push_back(wa_path + "\\User\\Speech");
	
	char drive_root[] = "A:\\";
	
	FIND_CD:
	
	while(drive_root[0] <= 'Z')
	{
		UINT type = GetDriveType(drive_root);
		
		char label[256];
		BOOL label_ok = GetVolumeInformation(drive_root, label, sizeof(label), NULL, NULL, NULL, NULL, 0);
		
		if(type == DRIVE_CDROM && label_ok && strcmp(label, "WA") == 0)
		{
			wav_search_path.push_back(std::string(drive_root) + "Data\\Streams");
			wav_search_path.push_back(std::string(drive_root) + "Data\\User\\Fanfare");
			wav_search_path.push_back(std::string(drive_root) + "Data\\User\\Speech");
			
			break;
		}
		
		drive_root[0]++;
	}
	
	if(drive_root[0] > 'Z')
	{
		if(MessageBox(NULL, "Could not find a WA CD-ROM. Some audio may be unavailable.\r\nTry again?", "Warning", MB_YESNO | MB_ICONWARNING | MB_TASKMODAL) == IDYES)
		{
			drive_root[0] = 'A';
			goto FIND_CD;
		}
	}
}

/* Hash a wav file for identification.
 * 
 * Only the first AUDIO_HASH_MAX_DATA bytes of the data section are hashed.
*/
uint32_t get_wav_file_hash(const char *path)
{
	SF_INFO info;
	memset(&info, 0, sizeof(info));
	
	SNDFILE *wav = sf_open(path, SFM_READ, &info);
	if(!wav)
	{
		return 0;
	}
	
	char buf[AUDIO_HASH_MAX_DATA];
	sf_count_t size = sf_read_raw(wav, buf, AUDIO_HASH_MAX_DATA);
	
	sf_close(wav);
	
	uint32_t hash;
	MurmurHash3_x86_32(buf, size, 0, &hash);
	
	return hash;
}

/* Recursively search a directory for a wav file with the given hash.
 * 
 * Loads the file, adds it to the wav_files cache and returns a pointer to the
 * wav_file structure on success.
 * 
 * Returns NULL if the file was not found or could not be loaded.
*/
wav_file *wav_search(uint32_t hash, std::string path)
{
	WIN32_FIND_DATA node;
	
	HANDLE dir = FindFirstFile(std::string(path + "\\*").c_str(), &node);
	
	while(dir != INVALID_HANDLE_VALUE)
	{
		std::string name  = node.cFileName;
		std::string npath = path + "\\" + name;
		
		if(name == "." || name == "..")
		{
			
		}
		else if(node.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
		{
			wav_file *r = wav_search(hash, npath);
			
			if(r)
			{
				FindClose(dir);
				return r;
			}
		}
		else if(strcasecmp(node.cFileName + strlen(node.cFileName) - 4, ".wav") == 0)
		{
			uint32_t file_hash = get_wav_file_hash(npath.c_str());
			
			if(file_hash && hash == file_hash)
			{
				FindClose(dir);
				
				return &(wav_files.insert(std::make_pair(hash, wav_file(npath))).first->second);
			}
		}
		
		if(!FindNextFile(dir, &node))
		{
			FindClose(dir);
			break;
		}
	}
	
	return NULL;
}

wav_file *get_wav_file(uint32_t hash)
{
	std::map<uint32_t, wav_file>::iterator w = wav_files.find(hash);
	
	if(w != wav_files.end())
	{
		return &(w->second);
	}
	
	for(size_t i = 0; i < wav_search_path.size(); i++)
	{
		wav_file *file = wav_search(hash, wav_search_path[i]);
		
		if(file)
		{
			return file;
		}
	}
	
	return NULL;
}

/* Returns true if any of the samples in the given PCM data are at the minimum
 * or maximum representable values.
*/
bool pcm_contains_clipping(const void *data, size_t samples, int bits)
{
	uint8_t *data_8  = (uint8_t*)(data);
	int16_t *data_16 = (int16_t*)(data);
	
	for(size_t i = 0; i < samples; i++)
	{
		if(bits == 8)
		{
			uint8_t sample = *(data_8++);
			
			if(sample == 0 || sample == UINT8_MAX)
			{
				return true;
			}
		}
		else if(bits == 16)
		{
			int16_t sample = *(data_16++);
			
			if(sample == INT16_MIN || sample == INT16_MAX)
			{
				return true;
			}
		}
	}
	
	return false;
}

struct audio_handle
{
	wav_file *file;
	
	ga_Handle *handle;
	
	audio_handle(ga_Mixer *mixer, wav_file *_file, int volume)
	{
		file = _file;
		
		assert((handle = gau_create_handle_sound(mixer, file->sound, NULL, NULL, NULL)));
		
		ga_handle_setParamf(handle, GA_HANDLE_PARAM_GAIN, (gc_float32)(volume) / 100);
	}
};

void destroy_ga_handles(std::map<unsigned int, audio_handle> &handles)
{
	for(std::map<unsigned int, audio_handle>::iterator h = handles.begin(); h != handles.end(); h++)
	{
		ga_handle_destroy(h->second.handle);
	}
	
	handles.clear();
}

bool make_output_wav()
{
	std::string log_path = config.capture_dir + "\\" FRAME_PREFIX "audio.dat";
	std::string wav_path = config.capture_dir + "\\" FRAME_PREFIX "audio.wav";
	
	FILE *log = fopen(log_path.c_str(), "rb");
	if(!log)
	{
		log_push(std::string("Could not open " FRAME_PREFIX "audio.dat: ") + w32_error(GetLastError()) + "\r\n");
		return false;
	}
	
	SF_INFO out_fmt;
	
	out_fmt.samplerate = SAMPLE_RATE;
	out_fmt.channels   = CHANNELS;
	out_fmt.format     = SF_FORMAT_WAV | SF_FORMAT_PCM_16;
	
	SNDFILE *out_wav = sf_open(wav_path.c_str(), SFM_WRITE, &out_fmt);
	assert(out_wav);
	
	struct ga_Format mixer_fmt;
	
	mixer_fmt.sampleRate    = SAMPLE_RATE;
	mixer_fmt.bitsPerSample = SAMPLE_BITS;
	mixer_fmt.numChannels   = CHANNELS;
	
	ga_Mixer *mixer = ga_mixer_create(&mixer_fmt, SAMPLE_RATE / config.frame_rate);
	
	/* Buffer to hold one frame of mixed audio. */
	
	size_t frame_bytes = (SAMPLE_RATE / config.frame_rate) * (SAMPLE_BITS / 8) * CHANNELS;
	char *frame_buf    = new char[frame_bytes];
	
	int volume = config.init_vol;
	
	/* Map of buf_id values to ga_Handle objects. */
	
	std::map<unsigned int, audio_handle> handles;
	
	struct audio_event event;
	
	unsigned int frame_num = 0;
	
	RESTART:
	
	while(fread(&event, sizeof(event), 1, log))
	{
		assert(event.frame >= frame_num);
		
		/* Mix audio for any frames before this one. */
		
		while(frame_num < event.frame)
		{
			frame_num++;
			
			assert(ga_mixer_mix(mixer, frame_buf) == GC_SUCCESS);
			assert(sf_write_raw(out_wav, frame_buf, frame_bytes) == frame_bytes);
			
			size_t samples = (SAMPLE_RATE / config.frame_rate) * CHANNELS;
			
			if(config.fix_clipping && volume > config.min_vol && pcm_contains_clipping(frame_buf, samples, SAMPLE_BITS))
			{
				log_push("Clipping detected on frame " + to_string(frame_num) + " at " + to_string(volume) + "% volume\r\n");
				
				if((volume -= config.step_vol) < config.min_vol)
				{
					volume = config.min_vol;
				}
				
				log_push("Trying with " + to_string(volume) + "% volume...\r\n");
				
				/* Restart from the beginning with the new
				 * volume.
				*/
				
				frame_num = 0;
				
				assert(sf_seek(out_wav, 0, SEEK_SET) == 0);
				assert(fseek(log, 0, SEEK_SET) == 0);
				
				destroy_ga_handles(handles);
				
				goto RESTART;
			}
		}
		
		std::map<unsigned int, audio_handle>::iterator hi = handles.find(event.buf_id);
		
		if(event.op == AUDIO_OP_LOAD)
		{
			wav_file *wav = get_wav_file(event.arg);
			
			if(wav)
			{
				if(hi != handles.end())
				{
					ga_handle_destroy(hi->second.handle);
					handles.erase(hi);
				}
				
				handles.insert(std::make_pair(event.buf_id, audio_handle(mixer, wav, volume)));
			}
			else{
				log_push("Unknown WAV hash: " + to_string(event.arg) + "\r\n");
			}
		}
		else if(hi != handles.end())
		{
			audio_handle *h = &(hi->second);
			
			if(event.op == AUDIO_OP_FREE)
			{
				ga_handle_destroy(h->handle);
				handles.erase(hi);
			}
			else if(event.op == AUDIO_OP_CLONE)
			{  
				handles.insert(std::make_pair(event.arg, audio_handle(mixer, h->file, volume)));
			}
			else if(event.op == AUDIO_OP_START)
			{
				ga_handle_play(h->handle);
			}
			else if(event.op == AUDIO_OP_STOP)
			{
				ga_handle_stop(h->handle);
			}
			else if(event.op == AUDIO_OP_JMP)
			{
				ga_Format wav_fmt;
				ga_handle_format(h->handle, &wav_fmt);
				
				ga_handle_seek(h->handle, event.arg / (wav_fmt.bitsPerSample / 8));
			}
			else if(event.op == AUDIO_OP_FREQ)
			{
				ga_Format wav_fmt;
				ga_handle_format(h->handle, &wav_fmt);
				
				gc_float32 pitch = (gc_float32)(event.arg) / wav_fmt.sampleRate;
				
				ga_handle_setParamf(h->handle, GA_HANDLE_PARAM_PITCH, pitch);
			}
			else if(event.op == AUDIO_OP_VOLUME)
			{
				gc_float32 gain = ((gc_float32)(event.arg) / 10000) * ((gc_float32)(volume) / 100);
				
				ga_handle_setParamf(h->handle, GA_HANDLE_PARAM_GAIN, gain);
			}
		}
	}
	
	destroy_ga_handles(handles);
	
	delete frame_buf;
	
	ga_mixer_destroy(mixer);
	
	sf_close(out_wav);
	
	fclose(log);
	
	return true;
}
