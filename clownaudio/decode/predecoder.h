#pragma once

#include <stdbool.h>
#include <stddef.h>

#include "common.h"

typedef struct PredecoderData PredecoderData;
typedef struct Predecoder Predecoder;

PredecoderData* Predecoder_DecodeData(const unsigned char *data, size_t data_size);
void Predecoder_UnloadData(PredecoderData *data);
Predecoder* Predecoder_Create(PredecoderData *data, bool loop, DecoderInfo *info);
void Predecoder_Destroy(Predecoder *predecoder);
void Predecoder_Rewind(Predecoder *predecoder);
unsigned long Predecoder_GetSamples(Predecoder *predecoder, void *buffer, unsigned long frames_to_do);