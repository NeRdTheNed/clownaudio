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

#pragma once

//#include <stdbool.h>
#include <stddef.h>

#include "common.h"

typedef struct Decoder_libXMPLite Decoder_libXMPLite;

Decoder_libXMPLite* Decoder_libXMPLite_Create(const unsigned char *data, size_t data_size, bool loop, DecoderInfo *info);
void Decoder_libXMPLite_Destroy(Decoder_libXMPLite *decoder);
void Decoder_libXMPLite_Rewind(Decoder_libXMPLite *decoder);
size_t Decoder_libXMPLite_GetSamples(Decoder_libXMPLite *decoder, void *buffer, size_t frames_to_do);
