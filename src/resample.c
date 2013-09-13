/* PCM resampling function
 * By Daniel Collins (2013)
 * 
 * The algorithm used here is rather crude and skews the speed of the audio when
 * the in/out sample rates aren't a multiple of each other.
 * 
 * Supports 8 bit unsigned and 16 bit signed samples at arbitrary bit rates as
 * both input and output.
 * 
 * This can be compiled as a standalone test program (requires sndfile) using
 * the following command (or similar):
 * 
 * gcc -std=c99 -Wall -o test -DTEST resample.c -lsndfile
 * 
 * See the main() function near the bottom for a usage example.
*/

#include <stdlib.h>
#include <stdint.h>
#include <assert.h>

/* Macros to deal with reading/writing samples from/to memory using an offset
 * measured in samples.
 * 
 * Samples which are being worked with are held in 16-bit signed form.
*/

#define SAMPLE_READ(buf, bits, off) ( \
	bits == 8 \
		? (*((uint8_t*)(buf) + (off)) - 128) * 256 \
		: *((int16_t*)(buf) + (off)))

#define SAMPLE_WRITE(buf, bits, off, sample) \
	if(bits == 8) \
	{ \
		*((uint8_t*)(buf) + (off)) = ((sample) / 256) + 128; \
	} \
	else{ \
		*((int16_t*)(buf) + (off)) = (sample); \
	}

unsigned int pcm_resample_frames(unsigned int frames, int rate_in, int rate_out)
{
	return frames * ((double)(rate_out) / rate_in);
}

unsigned int pcm_resample_bufsize(unsigned int frames, int channels, int rate_in, int rate_out, int bits_out)
{
	unsigned int out_frames = pcm_resample_frames(frames, rate_in, rate_out);
	unsigned int pcm_size   = out_frames * (bits_out / 8) * channels;
	
	return pcm_size;
}

void pcm_resample(const void *pcm_in, const void *pcm_out, unsigned int frames, int channels, int rate_in, int rate_out, int bits_in, int bits_out)
{
	/* Do a few sanity checks. */
	
	assert(rate_in  > 0);
	assert(rate_out > 0);
	
	assert(bits_in  == 8 || bits_in  == 16);
	assert(bits_out == 8 || bits_out == 16);
	
	/* Decide how many samples in the input/output buffers resolves to a
	 * single sample in the other. The lower rate buffer will be 1.
	*/
	
	int in_step  = (rate_in  / rate_out); in_step  += !in_step;
	int out_step = (rate_out / rate_in);  out_step += !out_step;
	
	/* For each channel in the buffer... */
	
	for(int c_num = 0; c_num < channels; c_num++)
	{
		/* ...start at its first sample... */
		
		for(unsigned int ip = c_num, op = c_num; ip < frames * channels;)
		{
			/* ...average in_step samples from the input buffer in
			 * preparation for writing to the output buffer...
			*/
			
			int sample = 0, x;
			
			for(x = 0; x < in_step && ip < frames * channels; ++x, ip += channels)
			{
				sample += SAMPLE_READ(pcm_in, bits_in, ip);
			}
			
			sample /= x;
			
			/* ...write the sample out_step times... */
			
			for(x = 0; x < out_step; ++x, op += channels)
			{
				SAMPLE_WRITE(pcm_out, bits_out, op, sample);
			}
		}
		
		/* Smooth out the flat points created by stretching a single
		 * sample over multiple in the output.
		 * 
		 * TODO: Base from the center of the sample rather than the
		 * edge.
		*/
		
		if(out_step > 1)
		{
			for(unsigned int op = c_num, f = 0; f + 1 < frames; ++f)
			{
				int base = SAMPLE_READ(pcm_out, bits_out, op);
				int next = SAMPLE_READ(pcm_out, bits_out, op + channels * out_step);
				
				int sub = (base - next) / out_step;
				
				op += channels;
				
				for(int x = 1; x < out_step; ++x, op += channels)
				{
					base -= sub;
					SAMPLE_WRITE(pcm_out, bits_out, op, base);
				}
			}
		}
	}
}

#ifdef TEST

#include <stdio.h>
#include <sndfile.h>
#include <string.h>

int main(int argc, char **argv)
{
	if(argc < 3 || argc > 5 || (argc >= 4 && (argv[3][0] == '\0' || argv[3][strspn(argv[3], "1234567890")])) || (argc >= 5 && strcmp(argv[4], "8") && strcmp(argv[4], "16")))
	{
		fprintf(stderr, "Usage: %s <input file> <output file> [output rate (Hz)] [output bits (8 or 16)]\n", argv[0]);
		return 1;
	}
	
	/* Read... */
	
	SF_INFO in_fmt;
	in_fmt.format = 0;
	
	SNDFILE *in_wav = sf_open(argv[1], SFM_READ, &in_fmt);
	assert(in_wav);
	
	int in_bits    = in_fmt.format & SF_FORMAT_PCM_U8 ? 8 : 16;
	size_t in_size = in_fmt.frames * in_fmt.channels * (in_bits / 8);
	
	void *in_buf = malloc(in_size);
	assert(in_buf);
	
	assert(sf_read_raw(in_wav, in_buf, in_size) == in_size);
	
	sf_close(in_wav);
	
	/* Resample... */
	
	int rate_out = (argc >= 4 ? atoi(argv[3]) : 44100);
	int bits_out = (argc >= 5 ? atoi(argv[4]) : 16);
	
	size_t out_size = pcm_resample_bufsize(in_fmt.frames, in_fmt.channels, in_fmt.samplerate, rate_out, bits_out);
	
	void *out_buf = malloc(out_size);
	assert(out_buf);
	
	memset(out_buf, 0xFF, out_size);
	
	pcm_resample(in_buf, out_buf, in_fmt.frames, in_fmt.channels, in_fmt.samplerate, rate_out, in_bits, bits_out);
	free(in_buf);
	
	/* Write... */
	
	SF_INFO out_fmt;
	
	out_fmt.samplerate = rate_out;
	out_fmt.channels   = in_fmt.channels;
	out_fmt.format     = SF_FORMAT_WAV | (bits_out == 8 ? SF_FORMAT_PCM_U8 : SF_FORMAT_PCM_16);
	
	SNDFILE *out_wav = sf_open(argv[2], SFM_WRITE, &out_fmt);
	assert(out_wav);
	
	assert(sf_write_raw(out_wav, out_buf, out_size) == out_size);
	free(out_buf);
	
	sf_close(out_wav);
	
	return 0;
}

#endif
