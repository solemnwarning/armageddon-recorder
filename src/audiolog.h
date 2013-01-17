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

#ifndef AREC_AUDIOLOG_H
#define AREC_AUDIOLOG_H

/* Prefix for exported frames and audio. */
#define FRAME_PREFIX "arec_"

/* Maximum length of PCM data to hash. */
#define AUDIO_HASH_MAX_DATA 1024

#define AUDIO_OP_INIT   1
#define AUDIO_OP_FREE   2
#define AUDIO_OP_CLONE  3
#define AUDIO_OP_LOAD   4
#define AUDIO_OP_START  5
#define AUDIO_OP_STOP   6
#define AUDIO_OP_JMP    7
#define AUDIO_OP_FREQ   8
#define AUDIO_OP_VOLUME 9

#define AUDIO_FLAG_REPEAT ((unsigned int)(1) << 0)

typedef struct audio_event audio_event;

struct audio_event
{
	unsigned int buf_id;
	unsigned int frame;
	
	unsigned int op;
	unsigned int arg;
};

#endif /* !AREC_AUDIOLOG_H */
