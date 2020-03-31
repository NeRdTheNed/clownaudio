/*
 *  (C) 2018-2020 Clownacy
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

#include "clownaudio/playback.h"

#include <stddef.h>
#include <stdlib.h>

#ifndef MINIAUDIO_ENABLE_DEVICE_IO
 #define MA_NO_DEVICE_IO
#endif
#include "../miniaudio.h"

struct ClownAudio_Stream
{
	void (*user_callback)(void*, float*, size_t);
	void *user_data;

	ma_device device;
};

static void Callback(ma_device *device, void *output_buffer_void, const void *input_buffer, ma_uint32 frames_to_do)
{
	(void)input_buffer;

	ClownAudio_Stream *stream = (ClownAudio_Stream*)device->pUserData;
	float *output_buffer = (float*)output_buffer_void;

	stream->user_callback(stream->user_data, output_buffer, frames_to_do);
}

CLOWNAUDIO_EXPORT bool ClownAudio_InitPlayback(void)
{
	return true;
}

CLOWNAUDIO_EXPORT void ClownAudio_DeinitPlayback(void)
{
	
}

CLOWNAUDIO_EXPORT ClownAudio_Stream* ClownAudio_CreateStream(unsigned long *sample_rate, void (*user_callback)(void*, float*, size_t))
{
	ClownAudio_Stream *stream = (ClownAudio_Stream*)malloc(sizeof(ClownAudio_Stream));

	if (stream != NULL)
	{
		ma_device_config config = ma_device_config_init(ma_device_type_playback);
		config.playback.pDeviceID = NULL;
		config.playback.format = ma_format_f32;
		config.playback.channels = 2;
		config.sampleRate = 0;	// Use native sample rate
		config.noPreZeroedOutputBuffer = MA_TRUE;
		config.dataCallback = Callback;
		config.pUserData = stream;

		if (ma_device_init(NULL, &config, &stream->device) == MA_SUCCESS)
		{
			if (sample_rate != NULL)
				*sample_rate = stream->device.sampleRate;

			stream->user_callback = user_callback;
			stream->user_data = NULL;

			return stream;
		}

		free(stream);
	}

	return NULL;
}

CLOWNAUDIO_EXPORT bool ClownAudio_DestroyStream(ClownAudio_Stream *stream)
{
	if (stream != NULL)
	{
		ma_device_uninit(&stream->device);
		free(stream);
	}

	return true;
}

CLOWNAUDIO_EXPORT void ClownAudio_SetStreamCallbackData(ClownAudio_Stream *stream, void *user_data)
{
	if (stream != NULL)
		stream->user_data = user_data;
}

CLOWNAUDIO_EXPORT bool ClownAudio_PauseStream(ClownAudio_Stream *stream)
{
	bool success = true;

	if (stream != NULL && ma_device_is_started(&stream->device))
		success = ma_device_stop(&stream->device) == MA_SUCCESS;

	return success;
}

CLOWNAUDIO_EXPORT bool ClownAudio_ResumeStream(ClownAudio_Stream *stream)
{
	bool success = true;

	if (stream != NULL && !ma_device_is_started(&stream->device))
		success = ma_device_start(&stream->device) == MA_SUCCESS;

	return success;
}
