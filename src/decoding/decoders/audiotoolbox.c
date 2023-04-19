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

#include "audiotoolbox.h"

#ifndef __cplusplus
#include <stdbool.h>
#endif
#include <stddef.h>
#include <stdlib.h>

#include <AudioToolbox/AudioToolbox.h>

#include "common.h"

typedef struct Decoder_AudioToolbox_callback_data
{
	size_t data_size;
	const unsigned char *data;
} Decoder_AudioToolbox_callback_data;

typedef struct Decoder_AudioToolbox
{
	uint32_t bytes_per_frame;
	unsigned int channel_count;
	AudioFileID audio_file;
	ExtAudioFileRef audio_file_ext;
	Decoder_AudioToolbox_callback_data *decoder_callback_data;
} Decoder_AudioToolbox;

static OSStatus audio_file_read_callback(void *inClientData, SInt64 inPosition, UInt32 requestCount, void *buffer, UInt32 *actualCount)
{
	size_t bytes_read;
	Decoder_AudioToolbox_callback_data *decoder_callback_data = (Decoder_AudioToolbox_callback_data*)inClientData;

	if(inPosition < decoder_callback_data->data_size)
	{
		size_t bytes_available = decoder_callback_data->data_size - inPosition;
		bytes_read = requestCount <= bytes_available ? requestCount : bytes_available;
		memcpy(buffer, decoder_callback_data->data + inPosition, bytes_read);
	}
	else
		bytes_read = 0;

	if (actualCount != NULL)
		*actualCount = bytes_read;

	return 0;
}

static SInt64 audio_file_get_size_callback(void *inClientData)
{
	Decoder_AudioToolbox_callback_data *decoder_callback_data = (Decoder_AudioToolbox_callback_data*)inClientData;

	return decoder_callback_data->data_size;
}

void* Decoder_AUDIOTOOLBOX_Create(const unsigned char *data, size_t data_size, bool loop, const DecoderSpec *wanted_spec, DecoderSpec *spec)
{
	Decoder_AudioToolbox_callback_data *decoder_callback_data = (Decoder_AudioToolbox_callback_data*)malloc(sizeof(Decoder_AudioToolbox_callback_data));

	(void)loop;

	if (decoder_callback_data != NULL)
	{
		AudioFileID audio_file;
		OSStatus error;

		decoder_callback_data->data = data;
		decoder_callback_data->data_size = data_size;

		error = AudioFileOpenWithCallbacks((void*)decoder_callback_data, audio_file_read_callback, NULL, audio_file_get_size_callback, NULL, 0, &audio_file);

		if (!error)
		{
			ExtAudioFileRef audio_file_ext;

			error = ExtAudioFileWrapAudioFileID(audio_file, false, &audio_file_ext);

			if(!error)
			{
				AudioStreamBasicDescription input_file_format;

				UInt32 property_size = sizeof(AudioStreamBasicDescription);

				error = ExtAudioFileGetProperty(audio_file_ext, kExtAudioFileProperty_FileDataFormat, &property_size, &input_file_format);

				if(!error)
				{
					AudioStreamBasicDescription output_format;
					output_format.mFormatID = kAudioFormatLinearPCM;
					output_format.mFormatFlags = kLinearPCMFormatFlagIsSignedInteger | kLinearPCMFormatFlagIsPacked
				#if defined(__ppc64__) || defined(__ppc__)
					                             | kAudioFormatFlagIsBigEndian
				#endif
					                             ;
				#ifdef CLOWNAUDIO_AUDIOTOOLBOX_USE_APPLE_RESAMPLER
					/* Let AudioToolbox handle resampling */
					output_format.mSampleRate = wanted_spec->sample_rate == 0 ? input_file_format.mSampleRate : wanted_spec->sample_rate;
				#else
					output_format.mSampleRate = input_file_format.mSampleRate;
				#endif
					output_format.mBitsPerChannel = 16;
					/* Clamp output channels to wanted channels, as otherwise an ear-splitting tone plays for files with more than 2 channels */
					output_format.mChannelsPerFrame = input_file_format.mChannelsPerFrame > wanted_spec->channel_count ? wanted_spec->channel_count : input_file_format.mChannelsPerFrame;
					/* kAudioFormatLinearPCM doesn't use packets */
					output_format.mFramesPerPacket = 1;
					/* Bytes per channel * channels per frame */
					output_format.mBytesPerFrame = (output_format.mBitsPerChannel / 8) * output_format.mChannelsPerFrame;
					/* Bytes per frame * frames per packet */
					output_format.mBytesPerPacket = output_format.mBytesPerFrame * output_format.mFramesPerPacket;

					error = ExtAudioFileSetProperty(audio_file_ext, kExtAudioFileProperty_ClientDataFormat, sizeof(AudioStreamBasicDescription), &output_format);

					if(!error)
					{
						Decoder_AudioToolbox *decoder = (Decoder_AudioToolbox*)malloc(sizeof(Decoder_AudioToolbox));

						if (decoder != NULL)
						{
							decoder->bytes_per_frame = output_format.mBytesPerFrame;
							decoder->channel_count = output_format.mChannelsPerFrame;
							decoder->audio_file = audio_file;
							decoder->audio_file_ext = audio_file_ext;
							decoder->decoder_callback_data = decoder_callback_data;

							spec->sample_rate = output_format.mSampleRate;
							spec->channel_count = output_format.mChannelsPerFrame;
							spec->is_complex = false;

							return decoder;
						}
					}
				}

				ExtAudioFileDispose(audio_file_ext);
			}

			AudioFileClose(audio_file);
		}

		free(decoder_callback_data);
	}

	return NULL;
}

void Decoder_AUDIOTOOLBOX_Destroy(void *decoder_void)
{
	Decoder_AudioToolbox *decoder = (Decoder_AudioToolbox*)decoder_void;

	ExtAudioFileDispose(decoder->audio_file_ext);
	AudioFileClose(decoder->audio_file);

	free(decoder->decoder_callback_data);
	free(decoder);
}

void Decoder_AUDIOTOOLBOX_Rewind(void *decoder_void)
{
	Decoder_AudioToolbox *decoder = (Decoder_AudioToolbox*)decoder_void;

	ExtAudioFileSeek(decoder->audio_file_ext, 0);
}

size_t Decoder_AUDIOTOOLBOX_GetSamples(void *decoder_void, short *buffer, size_t frames_to_do)
{
	AudioBufferList buffer_list;
	UInt32 frames;
	Decoder_AudioToolbox *decoder = (Decoder_AudioToolbox*)decoder_void;

	frames = frames_to_do;

	buffer_list.mNumberBuffers = 1;
	buffer_list.mBuffers[0].mNumberChannels = decoder->channel_count;
	buffer_list.mBuffers[0].mDataByteSize = frames_to_do * decoder->bytes_per_frame;
	buffer_list.mBuffers[0].mData = buffer;

	ExtAudioFileRead(decoder->audio_file_ext, &frames, &buffer_list);

	return frames;
}
