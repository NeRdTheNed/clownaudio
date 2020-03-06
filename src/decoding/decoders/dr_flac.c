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

#include "dr_flac.h"

//#include <stdbool.h>
#include "bool.h"
#include <stddef.h>

#define DR_FLAC_IMPLEMENTATION
#define DR_FLAC_NO_STDIO

#include "libs/dr_flac.h"

#include "common.h"

Decoder_DR_FLAC* Decoder_DR_FLAC_Create(const unsigned char *data, size_t data_size, bool loop, DecoderInfo *info)
{
	(void)loop;	/* This is ignored in simple decoders */

	drflac *backend = drflac_open_memory(data, data_size, NULL);

	if (backend != NULL)
	{
		info->sample_rate = backend->sampleRate;
		info->channel_count = backend->channels;
		info->format = DECODER_FORMAT_S32;
		info->is_complex = false;
	}

	return (Decoder_DR_FLAC*)backend;
}

void Decoder_DR_FLAC_Destroy(Decoder_DR_FLAC *decoder)
{
	drflac_close((drflac*)decoder);
}

void Decoder_DR_FLAC_Rewind(Decoder_DR_FLAC *decoder)
{
	drflac_seek_to_pcm_frame((drflac*)decoder, 0);
}

size_t Decoder_DR_FLAC_GetSamples(Decoder_DR_FLAC *decoder, void *buffer, size_t frames_to_do)
{
	return drflac_read_pcm_frames_s32((drflac*)decoder, frames_to_do, buffer);
}
