/* Armageddon Recorder
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

#ifndef DSOUND_CAPTURE_H
#define DSOUND_CAPTURE_H

#define AUDIO_OP_INIT  1
#define AUDIO_OP_FREE  2
#define AUDIO_OP_CLONE 3
#define AUDIO_OP_LOAD  4
#define AUDIO_OP_START 5
#define AUDIO_OP_STOP  6
#define AUDIO_OP_JMP   7
#define AUDIO_OP_FREQ  8
#define AUDIO_OP_GAIN  9

typedef struct audio_event audio_event;

struct audio_event
{
	unsigned int check;
	
	unsigned int frame;
	unsigned int op;
	
	union {
		struct {
			unsigned int buf_id;
			unsigned int size;
			
			unsigned int sample_rate;
			unsigned int sample_bits;
			unsigned int channels;
		} init;
		
		struct {
			unsigned int buf_id;
		} free;
		
		struct {
			unsigned int src_buf_id;
			unsigned int new_buf_id;
		} clone;
		
		struct {
			unsigned int buf_id;
			unsigned int offset;
			unsigned int size;
		} load;
		
		struct {
			unsigned int  buf_id;
			unsigned char loop;
		} start;
		
		struct {
			unsigned int buf_id;
		} stop;
		
		struct {
			unsigned int buf_id;
			unsigned int offset;
		} jmp;
		
		struct {
			unsigned int buf_id;
			unsigned int sample_rate;
		} freq;
		
		struct {
			unsigned int buf_id;
			double gain;
		} gain;
	} e;
};

#endif /* !DSOUND_CAPTURE_H */
