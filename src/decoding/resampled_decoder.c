// Copyright (c) 2019-2022 Clownacy
//
// This software is provided 'as-is', without any express or implied
// warranty.  In no event will the authors be held liable for any damages
// arising from the use of this software.
//
// Permission is granted to anyone to use this software for any purpose,
// including commercial applications, and to alter it and redistribute it
// freely, subject to the following restrictions:
//
// 1. The origin of this software must not be misrepresented; you must not
//    claim that you wrote the original software. If you use this software
//    in a product, an acknowledgment in the product documentation would be
//    appreciated but is not required.
// 2. Altered source versions must be plainly marked as such, and must not be
//    misrepresented as being the original software.
// 3. This notice may not be removed or altered from any source distribution.

#include "resampled_decoder.h"

#ifndef __cplusplus
#include <stdbool.h>
#endif
#include <stddef.h>
#include <stdlib.h>

#ifdef CLOWNAUDIO_CLOWNRESAMPLER
 #define CLOWNRESAMPLER_IMPLEMENTATION
 #define CLOWNRESAMPLER_STATIC
 #define CLOWNRESAMPLER_NO_HIGH_LEVEL_RESAMPLE_END
 #include "clownresampler/clownresampler.h"
#else
 #define MA_NO_DECODING
 #define MA_NO_ENCODING
 #define MA_NO_WAV
 #define MA_NO_FLAC
 #define MA_NO_MP3
 #define MA_NO_GENERATION

 #ifndef MINIAUDIO_ENABLE_DEVICE_IO
  #define MA_NO_DEVICE_IO
  #define MA_NO_THREADING
 #endif

 #include "../miniaudio.h"
#endif

#include "decoders/common.h"

#ifdef CLOWNAUDIO_CLOWNRESAMPLER
#else
#define RESAMPLE_BUFFER_SIZE 0x1000
#endif

typedef struct ResampledDecoder
{
	DecoderStage next_stage;
	unsigned long in_sample_rate;
	unsigned long out_sample_rate;
	unsigned long in_sample_rate_scaled;
	unsigned long low_pass_filter_sample_rate;
	size_t in_channel_count;
	size_t out_channel_count;
#ifdef CLOWNAUDIO_CLOWNRESAMPLER
	ClownResampler_HighLevel_State clownresampler_state;
#else
	ma_data_converter converter;
	short buffer[RESAMPLE_BUFFER_SIZE];
	size_t buffer_end;
	size_t buffer_done;
#endif
} ResampledDecoder;

#ifdef CLOWNAUDIO_CLOWNRESAMPLER
typedef struct ResamplerCallbackData
{
	ResampledDecoder *resampled_decoder;
	short *output_pointer;
	size_t output_buffer_frames_remaining;
} ResamplerCallbackData;
#endif

#ifdef CLOWNAUDIO_CLOWNRESAMPLER
static ClownResampler_Precomputed clownresampler_precomputed;
static bool clownresampler_precomputed_done;

static size_t ResamplerInputCallback(void* const user_data, cc_s16l* const buffer, const size_t buffer_size)
{
	const ResamplerCallbackData* const callback_data = (ResamplerCallbackData*)user_data;
	ResampledDecoder* const resampled_decoder = callback_data->resampled_decoder;

	short temporary_buffer[0x400];

	short *selected_buffer;
	size_t selected_buffer_size;

	if (resampled_decoder->in_channel_count == resampled_decoder->out_channel_count)
	{
		selected_buffer = buffer;
		selected_buffer_size = buffer_size;
	}
	else
	{
		selected_buffer = temporary_buffer;
		selected_buffer_size = CLOWNRESAMPLER_MIN(buffer_size, CLOWNRESAMPLER_COUNT_OF(temporary_buffer) / resampled_decoder->in_channel_count);
	}

	const size_t frames_read = resampled_decoder->next_stage.GetSamples(resampled_decoder->next_stage.decoder, selected_buffer, selected_buffer_size);

	const short *in_buffer_pointer = selected_buffer;
	short *out_buffer_pointer = buffer;

	if (resampled_decoder->in_channel_count == 2 && resampled_decoder->out_channel_count == 1)
	{
		// Downmix stereo to mono
		for (size_t i = 0; i < frames_read; ++i)
		{
			*out_buffer_pointer = *in_buffer_pointer++ / 2;
			*out_buffer_pointer += *in_buffer_pointer++ / 2;
			++out_buffer_pointer;
		}
	}
	else if (resampled_decoder->in_channel_count == 1 && resampled_decoder->out_channel_count == 2)
	{
		// Upmix mono to stereo
		for (size_t i = 0; i < frames_read; ++i)
		{
			*out_buffer_pointer++ = *in_buffer_pointer;
			*out_buffer_pointer++ = *in_buffer_pointer;
			++in_buffer_pointer;
		}
	}

	return frames_read;
}

static cc_bool ResamplerOutputCallback(void* const user_data, const cc_s32f* const frame, const cc_u8f total_samples)
{
	ResamplerCallbackData* const callback_data = (ResamplerCallbackData*)user_data;

	cc_u8f i;

	/* Output the frame. */
	for (i = 0; i < total_samples; ++i)
	{
		cc_s32f sample = frame[i];

		/* Clamp the sample to 16-bit. */
		if (sample > 0x7FFF)
			sample = 0x7FFF;
		else if (sample < -0x7FFF)
			sample = -0x7FFF;

		/* Push the sample to the output buffer. */
		*callback_data->output_pointer++ = (short)sample;
	}

	/* Signal whether there is more room in the output buffer. */
	return --callback_data->output_buffer_frames_remaining != 0;
}

#endif

void* ResampledDecoder_Create(DecoderStage *next_stage, bool dynamic_sample_rate, const DecoderSpec *wanted_spec, const DecoderSpec *child_spec)
{
#ifdef CLOWNAUDIO_CLOWNRESAMPLER
	if (!clownresampler_precomputed_done)
	{
		clownresampler_precomputed_done = true;

		ClownResampler_Precompute(&clownresampler_precomputed);
	}
#endif

//	DecoderSpec child_spec;
//	void *decoder = DecoderSelector_Create(data, loop, wanted_spec, &child_spec);

//	if (decoder != NULL)
	{
		ResampledDecoder *resampled_decoder = (ResampledDecoder*)malloc(sizeof(ResampledDecoder));

		if (resampled_decoder != NULL)
		{
			resampled_decoder->next_stage = *next_stage;
			resampled_decoder->in_sample_rate = child_spec->sample_rate;
			resampled_decoder->out_sample_rate = wanted_spec->sample_rate == 0 ? child_spec->sample_rate : wanted_spec->sample_rate;
			resampled_decoder->in_sample_rate_scaled = resampled_decoder->in_sample_rate;
			resampled_decoder->low_pass_filter_sample_rate = resampled_decoder->in_sample_rate < resampled_decoder->out_sample_rate ? resampled_decoder->in_sample_rate : resampled_decoder->out_sample_rate;
			resampled_decoder->in_channel_count = child_spec->channel_count;
			resampled_decoder->out_channel_count = wanted_spec->channel_count;

		#ifdef CLOWNAUDIO_CLOWNRESAMPLER
			if (dynamic_sample_rate)
			{
				/* Set a wide sample rate ratio so that the low-pass filter can be wildly-adjusted. */
				if (ClownResampler_HighLevel_Init(&resampled_decoder->clownresampler_state, resampled_decoder->out_channel_count, 0x100, 1, resampled_decoder->low_pass_filter_sample_rate))
				{
					/* Apply the actual sample rates. */
					if (ClownResampler_HighLevel_Adjust(&resampled_decoder->clownresampler_state, resampled_decoder->in_sample_rate_scaled, resampled_decoder->out_sample_rate, resampled_decoder->low_pass_filter_sample_rate))
						return resampled_decoder;
				}
			}
			else
			{
				/* Just do things the normal way. */
				if (ClownResampler_HighLevel_Init(&resampled_decoder->clownresampler_state, resampled_decoder->out_channel_count, resampled_decoder->in_sample_rate_scaled, resampled_decoder->out_sample_rate, resampled_decoder->low_pass_filter_sample_rate))
					return resampled_decoder;
			}
		#else
			ma_data_converter_config config = ma_data_converter_config_init(ma_format_s16, ma_format_s16, child_spec->channel_count, wanted_spec->channel_count, resampled_decoder->in_sample_rate, resampled_decoder->out_sample_rate);

			if (dynamic_sample_rate)
				config.allowDynamicSampleRate = MA_TRUE;

			if (ma_data_converter_init(&config, NULL, &resampled_decoder->converter) == MA_SUCCESS)
			{
				resampled_decoder->buffer_end = 0;
				resampled_decoder->buffer_done = 0;

				return resampled_decoder;
			}
		#endif

			free(resampled_decoder);
		}
	}

	return NULL;
}

void ResampledDecoder_Destroy(void *resampled_decoder_void)
{
	ResampledDecoder *resampled_decoder = (ResampledDecoder*)resampled_decoder_void;

#ifdef CLOWNAUDIO_CLOWNRESAMPLER
#else
	ma_data_converter_uninit(&resampled_decoder->converter, NULL);
#endif
	resampled_decoder->next_stage.Destroy(resampled_decoder->next_stage.decoder);
	free(resampled_decoder);
}

void ResampledDecoder_Rewind(void *resampled_decoder_void)
{
	ResampledDecoder *resampled_decoder = (ResampledDecoder*)resampled_decoder_void;

	resampled_decoder->next_stage.Rewind(resampled_decoder->next_stage.decoder);
}

size_t ResampledDecoder_GetSamples(void *resampled_decoder_void, short *buffer, size_t frames_to_do)
{
	ResampledDecoder *resampled_decoder = (ResampledDecoder*)resampled_decoder_void;

#ifdef CLOWNAUDIO_CLOWNRESAMPLER
	ResamplerCallbackData callback_data;
	callback_data.resampled_decoder = resampled_decoder;
	callback_data.output_pointer = buffer;
	callback_data.output_buffer_frames_remaining = frames_to_do;

	ClownResampler_HighLevel_Resample(&resampled_decoder->clownresampler_state, &clownresampler_precomputed, ResamplerInputCallback, ResamplerOutputCallback, &callback_data);

	return frames_to_do - callback_data.output_buffer_frames_remaining;
#else
	size_t frames_done = 0;

	while (frames_done != frames_to_do)
	{
		if (resampled_decoder->buffer_done == resampled_decoder->buffer_end)
		{
			resampled_decoder->buffer_done = 0;

			resampled_decoder->buffer_end = resampled_decoder->next_stage.GetSamples(resampled_decoder->next_stage.decoder, resampled_decoder->buffer, RESAMPLE_BUFFER_SIZE / resampled_decoder->in_channel_count);

			if (resampled_decoder->buffer_end == 0)
				break;	// Sample end
		}

		ma_uint64 frames_in = (ma_uint64)(resampled_decoder->buffer_end - resampled_decoder->buffer_done);
		ma_uint64 frames_out = (ma_uint64)(frames_to_do - frames_done);
		ma_data_converter_process_pcm_frames(&resampled_decoder->converter, &resampled_decoder->buffer[resampled_decoder->buffer_done * resampled_decoder->in_channel_count], &frames_in, &buffer[frames_done * resampled_decoder->out_channel_count], &frames_out);

		resampled_decoder->buffer_done += (size_t)frames_in;
		frames_done += (size_t)frames_out;
	}

	return frames_done;
#endif
}

void ResampledDecoder_SetLoop(void *resampled_decoder_void, bool loop)
{
	ResampledDecoder *resampled_decoder = (ResampledDecoder*)resampled_decoder_void;

	resampled_decoder->next_stage.SetLoop(resampled_decoder->next_stage.decoder, loop);
}

void ResampledDecoder_SetSpeed(void *resampled_decoder_void, unsigned long speed)
{
	ResampledDecoder *resampled_decoder = (ResampledDecoder*)resampled_decoder_void;

	/* Perform a fixed-point multiplication in a way that prevents overflow. */
	const unsigned long upper_multiply = resampled_decoder->in_sample_rate * (speed >> 16);
	const unsigned long lower_multiply = resampled_decoder->in_sample_rate * (speed & 0xFFFF);
	resampled_decoder->in_sample_rate_scaled = upper_multiply + (lower_multiply >> 16);

#ifdef CLOWNAUDIO_CLOWNRESAMPLER
	ClownResampler_HighLevel_Adjust(&resampled_decoder->clownresampler_state, resampled_decoder->in_sample_rate_scaled, resampled_decoder->out_sample_rate, resampled_decoder->low_pass_filter_sample_rate);
#else
	ma_data_converter_set_rate(&resampled_decoder->converter, resampled_decoder->in_sample_rate_scaled, resampled_decoder->out_sample_rate);
#endif
}

void ResampledDecoder_SetLowPassFilter(void *resampled_decoder_void, unsigned long low_pass_filter_sample_rate)
{
	ResampledDecoder *resampled_decoder = (ResampledDecoder*)resampled_decoder_void;

	resampled_decoder->low_pass_filter_sample_rate = low_pass_filter_sample_rate;

#ifdef CLOWNAUDIO_CLOWNRESAMPLER
	ClownResampler_HighLevel_Adjust(&resampled_decoder->clownresampler_state, resampled_decoder->in_sample_rate_scaled, resampled_decoder->out_sample_rate, resampled_decoder->low_pass_filter_sample_rate);
#else
	ma_data_converter_set_rate(&resampled_decoder->converter, resampled_decoder->in_sample_rate_scaled, resampled_decoder->out_sample_rate);
#endif
}
