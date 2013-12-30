/* PCM resampling function
 *
 * Copyright (C) 2013 Daniel Collins <solemnwarning@solemnwarning.net>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *
 *     * Neither the name of the developer nor the names of its contributors
 *       may be used to endorse or promote products derived from this software
 *       without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE DEVELOPER BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#ifndef RESAMPLE_HPP
#define RESAMPLE_HPP

#include <math.h>
#include <vector>
#include <limits>
#include <algorithm>
#include <stdexcept>

template <typename InSample, typename OutSample, typename InputIterator> std::vector<OutSample> pcm_resample(const InputIterator begin, const InputIterator end, unsigned int channels, unsigned int rate_in, unsigned int rate_out)
{
	/* Determine the values used for silence in the sample types. */
	
	const InSample in_zero = std::numeric_limits<InSample>::is_signed
		? 0
		: ((std::numeric_limits<InSample>::max() / 2) + 1);
	
	const OutSample out_zero = std::numeric_limits<OutSample>::is_signed
		? 0
		: ((std::numeric_limits<OutSample>::max() / 2) + 1);
	
	/* Determine the number of input/output samples and frames. */
	
	size_t in_samples  = end - begin;
	size_t out_samples = in_samples * ((double)(rate_out) / rate_in);
	
	size_t in_frames  = in_samples / channels;
	size_t out_frames = out_samples / channels;
	
	if((in_samples % channels) != 0)
	{
		throw std::invalid_argument("number of samples must be a multiple of channels");
	}
	
	/* Fill the out vector with resampled samples. We iterate and address
	 * the input samples by frame/channel rather than sample to ensure any
	 * rounding errors cannot cause samples to slip between channels.
	*/
	
	std::vector<OutSample> out;
	out.reserve(out_samples);
	
	for(size_t f = 0; f < out_frames; ++f)
	{
		for(unsigned int c = 0; c < channels; ++c)
		{
			/* Calculate our position in the input buffer. */
			
			double in_frame = f / ((double)(rate_out) / rate_in);
			
			/* Get the samples from the input to contribute to this
			 * sample in the output.
			 *
			 * If rate_in is a multiple of rate_out, each sample in
			 * the output will fall directly on one in the input and
			 * if1/if2 will have the same value.
			 *
			 * Resampling may create a frame which exists beyond the
			 * end of the input, for this case if1/if2 are clamped
			 * to the last frame of the input, extending it.
			*/
			
			size_t if1 = std::min((size_t)(floor(in_frame)), in_frames - 1);
			size_t if2 = std::min((size_t)(ceil(in_frame)), in_frames - 1);
			
			InSample is1 = *(begin + ((channels * if1) + c));
			InSample is2 = *(begin + ((channels * if2) + c));
			
			/* Calculate the sample to write to the output buffer in
			 * the input format.
			 *
			 * This finds the value between is1/is2, proportional to
			 * how far each are from in_frame.
			*/
			
			InSample isc = is1 - ((is1 - is2) * (in_frame - if1));
			
			/* Convert the input sample to the output format and
			 * push it onto the output sample buffer.
			*/
			
			long double cs = isc - in_zero;
			
			cs *= (double)(std::numeric_limits<OutSample>::max() - out_zero) / (std::numeric_limits<InSample>::max() - in_zero);
			
			out.push_back((OutSample)(cs) + out_zero);
		}
	}
	
	return out;
}

#endif /* !RESAMPLE_HPP */
