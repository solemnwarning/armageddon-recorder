/* Armageddon Recorder - DirectSound log dumping tool
 * Copyright (C) 2013 Daniel Collins <solemnwarning@solemnwarning.net>
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

#include <stdio.h>
#include <assert.h>
#include <map>
#include <sndfile.h>

#include "ds-capture.h"

int main(int argc, char **argv)
{
	if(argc != 3)
	{
		fprintf(stderr, "Usage: %s <log path> <output directory>\n", argv[0]);
		return 1;
	}
	
	FILE *log = fopen(argv[1], "rb");
	assert(log);
	
	std::map<unsigned int, audio_event> buffers;
	unsigned int n = 0;
	
	audio_event event;
	while(fread(&event, sizeof(event), 1, log))
	{
		switch(event.op)
		{
			case AUDIO_OP_INIT:
			{
				buffers.insert(std::make_pair(event.e.init.buf_id, event));
				break;
			}
			
			case AUDIO_OP_FREE:
			{
				buffers.erase(event.e.free.buf_id);
				break;
			}
			
			case AUDIO_OP_CLONE:
			{
				std::map<unsigned int, audio_event>::iterator bi = buffers.find(event.e.clone.src_buf_id);
				
				if(bi == buffers.end())
				{
					fprintf(stderr, "Attempted to clone unknown buffer %u\n", event.e.clone.src_buf_id);
					break;
				}
				
				buffers.insert(std::make_pair(event.e.clone.new_buf_id, bi->second));
				
				break;
			}
			
			case AUDIO_OP_LOAD:
			{
				unsigned char *tmp = new unsigned char[event.e.load.size];
				
				if(fread(tmp, 1, event.e.load.size, log) != event.e.load.size)
				{
					fprintf(stderr, "Unexpected end of log\n");
					delete tmp;
					fclose(log);
					
					return 1;
				}
				
				std::map<unsigned int, audio_event>::iterator bi = buffers.find(event.e.load.buf_id);
				
				if(bi == buffers.end())
				{
					fprintf(stderr, "Attempted to load into unknown buffer %u\n", event.e.load.buf_id);
					delete tmp;
					
					break;
				}
				
				char path[1024];
				sprintf(path, "%s\\%04u-%08u.wav", argv[2], event.e.load.buf_id, ++n);
				
				SF_INFO wav_fmt;
				wav_fmt.samplerate = bi->second.e.init.sample_rate;
				wav_fmt.channels   = bi->second.e.init.channels;
				wav_fmt.format     = SF_FORMAT_WAV | (bi->second.e.init.sample_bits == 8 ? SF_FORMAT_PCM_U8 : SF_FORMAT_PCM_16);
				
				SNDFILE *wav = sf_open(path, SFM_WRITE, &wav_fmt);
				if(!wav)
				{
					fprintf(stderr, "Could not open %s: %s\n", path, sf_strerror(NULL));
					return false;
				}
				
				sf_write_raw(wav, tmp, event.e.load.size);
				
				sf_close(wav);
				delete tmp;
				
				break;
			}
		}
	}
	
	return 0;
}
