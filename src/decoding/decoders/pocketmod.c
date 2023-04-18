/*
Copyright (c) 2023 Ned Loynd

Permission to use, copy, modify, and/or distribute this software for any
purpose with or without fee is hereby granted.

THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES WITH
REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT,
INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR
OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
PERFORMANCE OF THIS SOFTWARE.
*/

#include "pocketmod.h"

#ifndef __cplusplus
#include <stdbool.h>
#endif
#include <stddef.h>
#include <stdlib.h>

#define POCKETMOD_INT_PCM
#define POCKETMOD_IMPLEMENTATION
#include "libs/pocketmod.h"

#include "common.h"

typedef struct Decoder_pocketmod
{
	bool should_loop;
	int current_loop;
	pocketmod_context context;
} Decoder_pocketmod;

void* Decoder_POCKETMOD_Create(const unsigned char *data, size_t data_size, bool loop, const DecoderSpec *wanted_spec, DecoderSpec *spec)
{
	Decoder_pocketmod *decoder = (Decoder_pocketmod*)calloc(1, sizeof(Decoder_pocketmod));

	if (decoder != NULL)
	{
		if (pocketmod_init(&decoder->context, data, data_size, wanted_spec->sample_rate))
		{
			decoder->should_loop = loop;
			decoder->current_loop = 0;
			spec->sample_rate = wanted_spec->sample_rate;
			spec->channel_count = 2;
			spec->is_complex = true;

			return decoder;
		}

		free(decoder);
	}

	return NULL;
}

void Decoder_POCKETMOD_Destroy(void *decoder_void)
{
	Decoder_pocketmod *decoder = (Decoder_pocketmod*)decoder_void;

	free(decoder);
}

void Decoder_POCKETMOD_Rewind(void *decoder_void)
{
	int initial_loop_count;
	float fake_buffer[1][2];
	Decoder_pocketmod *decoder = (Decoder_pocketmod*)decoder_void;

	initial_loop_count = pocketmod_loop_count(&decoder->context);

	/* Prevent rewinding from stopping a non-looped sound early */
	decoder->current_loop = initial_loop_count + 1;

	/* Render samples until the song loops */
	while (pocketmod_loop_count(&decoder->context) == initial_loop_count)
	{
		pocketmod_render(&decoder->context, fake_buffer, sizeof(fake_buffer));
	}
}

size_t Decoder_POCKETMOD_GetSamples(void *decoder_void, short *buffer, size_t frames_to_do)
{
	Decoder_pocketmod *decoder = (Decoder_pocketmod*)decoder_void;

	if (!decoder->should_loop && decoder->current_loop < pocketmod_loop_count(&decoder->context))
		return 0;

	return pocketmod_render(&decoder->context, buffer, frames_to_do * POCKETMOD_SAMPLE_SIZE) / POCKETMOD_SAMPLE_SIZE;
}
