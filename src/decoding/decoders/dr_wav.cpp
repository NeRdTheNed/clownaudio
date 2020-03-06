/*
 *  (C) 2019-2020 Clownacy
 *
 *  This software is provided 'as-is', without any express or implied
 *  warranty.  In no event will the authors be held liable for any damages
 *  arising from the use of this software.
 *
 *  Permission is granted to anyone to use this software for any purpose,
 *  including commercial applications, and to alter it and redistribute it
 *  freely, subject to the following restrictions:
 *
 *  1. The origin of this software must not be misrepresented; you must not
 *     claim that you wrote the original software. If you use this software
 *     in a product, an acknowledgment in the product documentation would be
 *     appreciated but is not required.
 *  2. Altered source versions must be plainly marked as such, and must not be
 *     misrepresented as being the original software.
 *  3. This notice may not be removed or altered from any source distribution.
 */

#include "dr_wav.h"

#include <stddef.h>
#include <stdlib.h>

#define DR_WAV_IMPLEMENTATION
#define DR_WAV_NO_STDIO

#include "libs/dr_wav.h"

#include "common.h"

Decoder_DR_WAV* Decoder_DR_WAV_Create(const unsigned char *data, size_t data_size, bool loop, DecoderInfo *info)
{
	(void)loop;	// This is ignored in simple decoders

	drwav *instance = (drwav*)malloc(sizeof(drwav));

	if (instance != NULL)
	{
		if (drwav_init_memory(instance, data, data_size, NULL))
		{
			info->sample_rate = instance->sampleRate;
			info->channel_count = instance->channels;
			info->format = DECODER_FORMAT_F32;
			info->is_complex = false;

			return (Decoder_DR_WAV*)instance;
		}

		free(instance);
	}

	return NULL;
}

void Decoder_DR_WAV_Destroy(Decoder_DR_WAV *decoder)
{
	drwav *instance = (drwav*)decoder;

	drwav_uninit(instance);
	free(instance);
}

void Decoder_DR_WAV_Rewind(Decoder_DR_WAV *decoder)
{
	drwav_seek_to_pcm_frame((drwav*)decoder, 0);
}

size_t Decoder_DR_WAV_GetSamples(Decoder_DR_WAV *decoder, void *buffer, size_t frames_to_do)
{
	return drwav_read_pcm_frames_f32((drwav*)decoder, frames_to_do, (float*)buffer);
}
