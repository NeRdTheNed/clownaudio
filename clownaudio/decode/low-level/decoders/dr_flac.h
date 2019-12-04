#pragma once

#include <stdbool.h>

#include "../../common.h"

typedef struct Decoder_DR_FLAC Decoder_DR_FLAC;

Decoder_DR_FLAC* Decoder_DR_FLAC_Create(const unsigned char *data, size_t data_size, bool loop, DecoderInfo *info);
void Decoder_DR_FLAC_Destroy(Decoder_DR_FLAC *decoder);
void Decoder_DR_FLAC_Rewind(Decoder_DR_FLAC *decoder);
unsigned long Decoder_DR_FLAC_GetSamples(Decoder_DR_FLAC *decoder, void *buffer, unsigned long frames_to_do);