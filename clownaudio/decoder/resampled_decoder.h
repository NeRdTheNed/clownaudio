#pragma once

#include <stdbool.h>
#include <stddef.h>

typedef struct ResampledDecoderData ResampledDecoderData;
typedef struct ResampledDecoder ResampledDecoder;

ResampledDecoderData* ResampledDecoder_LoadData(const unsigned char *file_buffer, size_t file_size, bool predecode);
void ResampledDecoder_UnloadData(ResampledDecoderData *data);
ResampledDecoder* ResampledDecoder_Create(ResampledDecoderData *data, bool loop, unsigned long sample_rate, unsigned int channel_count);
void ResampledDecoder_Destroy(ResampledDecoder *resampled_decoder);
void ResampledDecoder_Rewind(ResampledDecoder *resampled_decoder);
unsigned long ResampledDecoder_GetSamples(ResampledDecoder *resampled_decoder, void *buffer, unsigned long frames_to_do);
void ResampledDecoder_SetSampleRate(ResampledDecoder *resampled_decoder, unsigned long sample_rate);
