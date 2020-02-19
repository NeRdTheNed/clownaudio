#include "stb_vorbis.h"

#include <stdbool.h>
#include <stddef.h>

#define STB_VORBIS_IMPLEMENTATION
#define STB_VORBIS_NO_STDIO
#define STB_VORBIS_NO_PUSHDATA_API
#define STB_VORBIS_NO_INTEGER_CONVERSION

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#endif
#include "libs/stb_vorbis.c"
#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

#include "common.h"

Decoder* Decoder_STB_Vorbis_Create(const unsigned char *data, size_t data_size, bool loop, DecoderInfo *info)
{
	(void)loop;	// This is ignored in simple decoders

	stb_vorbis *instance = stb_vorbis_open_memory(data, data_size, NULL, NULL);

	if (instance != NULL)
	{
		const stb_vorbis_info vorbis_info = stb_vorbis_get_info(instance);

		info->sample_rate = vorbis_info.sample_rate;
		info->channel_count = vorbis_info.channels;
		info->format = DECODER_FORMAT_F32;
		info->complex = false;
	}

	return (Decoder*)instance;
}

void Decoder_STB_Vorbis_Destroy(Decoder *decoder)
{
	stb_vorbis_close((stb_vorbis*)decoder);
}

void Decoder_STB_Vorbis_Rewind(Decoder *decoder)
{
	stb_vorbis_seek_start((stb_vorbis*)decoder);
}

size_t Decoder_STB_Vorbis_GetSamples(Decoder *decoder, void *buffer, size_t frames_to_do)
{
	stb_vorbis *instance = (stb_vorbis*)decoder;

	return stb_vorbis_get_samples_float_interleaved(instance, instance->channels, buffer, frames_to_do * instance->channels);
}
