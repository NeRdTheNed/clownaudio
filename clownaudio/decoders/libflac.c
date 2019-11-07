#include "libflac.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include <FLAC/stream_decoder.h>

#include "common.h"
#include "../memory_stream.h"

#undef MIN
#define MIN(a, b) ((a) < (b) ? (a) : (b))

struct Decoder_libFLAC
{
	DecoderData *data;

	ROMemoryStream *memory_stream;
	FLAC__StreamDecoder *flac_stream_decoder;
	DecoderInfo *info;
	bool loops;

	unsigned int channel_count;

	bool error;

	unsigned int bits_per_sample;

	unsigned char *block_buffer;
	unsigned long block_buffer_index;
	unsigned long block_buffer_size;
};

static FLAC__StreamDecoderReadStatus MemoryFile_fread_wrapper(const FLAC__StreamDecoder *flac_stream_decoder, FLAC__byte *output, size_t *count, void *user)
{
	(void)flac_stream_decoder;

	Decoder_libFLAC *decoder = user;

	FLAC__StreamDecoderReadStatus status = FLAC__STREAM_DECODER_READ_STATUS_CONTINUE;

	*count = ROMemoryStream_Read(decoder->memory_stream, output, sizeof(FLAC__byte), *count);

	if (*count == 0)
		status = FLAC__STREAM_DECODER_READ_STATUS_END_OF_STREAM;

	return status;
}

static FLAC__StreamDecoderSeekStatus MemoryFile_fseek_wrapper(const FLAC__StreamDecoder *flac_stream_decoder, FLAC__uint64 offset, void *user)
{
	(void)flac_stream_decoder;

	Decoder_libFLAC *decoder = user;

	return ROMemoryStream_SetPosition(decoder->memory_stream, offset, MEMORYSTREAM_START) != 0 ? FLAC__STREAM_DECODER_SEEK_STATUS_ERROR : FLAC__STREAM_DECODER_SEEK_STATUS_OK;
}

static FLAC__StreamDecoderTellStatus MemoryFile_ftell_wrapper(const FLAC__StreamDecoder *flac_stream_decoder, FLAC__uint64 *offset, void *user)
{
	(void)flac_stream_decoder;

	Decoder_libFLAC *decoder = user;

	*offset = ROMemoryStream_GetPosition(decoder->memory_stream);

	return FLAC__STREAM_DECODER_TELL_STATUS_OK;
}

static FLAC__StreamDecoderLengthStatus MemoryFile_GetSize(const FLAC__StreamDecoder *flac_stream_decoder, FLAC__uint64 *length, void *user)
{
	(void)flac_stream_decoder;

	Decoder_libFLAC *decoder = user;

	const size_t old_offset = ROMemoryStream_GetPosition(decoder->memory_stream);

	ROMemoryStream_SetPosition(decoder->memory_stream, 0, MEMORYSTREAM_END);
	*length = ROMemoryStream_GetPosition(decoder->memory_stream);

	ROMemoryStream_SetPosition(decoder->memory_stream, old_offset, MEMORYSTREAM_START);

	return FLAC__STREAM_DECODER_LENGTH_STATUS_OK;
}

static FLAC__bool MemoryFile_EOF(const FLAC__StreamDecoder *flac_stream_decoder, void *user)
{
	(void)flac_stream_decoder;

	Decoder_libFLAC *decoder = user;

	const size_t offset = ROMemoryStream_GetPosition(decoder->memory_stream);

	ROMemoryStream_SetPosition(decoder->memory_stream, 0, MEMORYSTREAM_END);
	const size_t size = ROMemoryStream_GetPosition(decoder->memory_stream);

	ROMemoryStream_SetPosition(decoder->memory_stream, offset, MEMORYSTREAM_START);

	return (offset == size);
}

static FLAC__StreamDecoderWriteStatus WriteCallback(const FLAC__StreamDecoder *flac_stream_decoder, const FLAC__Frame *frame, const FLAC__int32* const buffer[], void *user)
{
	(void)flac_stream_decoder;

	Decoder_libFLAC *decoder = user;

	FLAC__int32 *block_buffer_pointer = (FLAC__int32*)decoder->block_buffer;
	for (unsigned int i = 0; i < frame->header.blocksize; ++i)
	{
		for (unsigned int j = 0; j < frame->header.channels; ++j)
		{
			const FLAC__int32 sample = buffer[j][i];

			// Downscale/upscale to 32-bit
			if (decoder->bits_per_sample < 32)
				*block_buffer_pointer++ = sample << (32 - decoder->bits_per_sample);
			else if (decoder->bits_per_sample > 32)
				*block_buffer_pointer++ = sample >> (decoder->bits_per_sample - 32);
			else
				*block_buffer_pointer++ = sample;
		}
	}

	decoder->block_buffer_index = 0;
	decoder->block_buffer_size = (block_buffer_pointer - (FLAC__int32*)decoder->block_buffer) / decoder->channel_count;

	return FLAC__STREAM_DECODER_WRITE_STATUS_CONTINUE;
}

static void MetadataCallback(const FLAC__StreamDecoder *flac_stream_decoder, const FLAC__StreamMetadata *metadata, void *user)
{
	(void)flac_stream_decoder;

	Decoder_libFLAC *decoder = user;

	decoder->info->sample_rate = metadata->data.stream_info.sample_rate;
	decoder->info->channel_count = decoder->channel_count = metadata->data.stream_info.channels;
	decoder->info->format = DECODER_FORMAT_S32;	// libFLAC doesn't do float32

	decoder->bits_per_sample = metadata->data.stream_info.bits_per_sample;

	// Init block buffer
	decoder->block_buffer = malloc(metadata->data.stream_info.max_blocksize * sizeof(FLAC__int32) * metadata->data.stream_info.channels);
	decoder->block_buffer_index = 0;
	decoder->block_buffer_size = 0;
}

static void ErrorCallback(const FLAC__StreamDecoder *flac_stream_decoder, FLAC__StreamDecoderErrorStatus status, void *user)
{
	(void)flac_stream_decoder;
	(void)status;

	Decoder_libFLAC *decoder = user;

	decoder->error = true;
}

Decoder_libFLAC* Decoder_libFLAC_Create(DecoderData *data, bool loops, unsigned long sample_rate, unsigned int channel_count, DecoderInfo *info)
{
	(void)sample_rate;
	(void)channel_count;

	Decoder_libFLAC *decoder = NULL;

	if (data != NULL)
	{
		decoder = malloc(sizeof(Decoder_libFLAC));

		if (decoder != NULL)
		{
			decoder->flac_stream_decoder = FLAC__stream_decoder_new();

			if (decoder->flac_stream_decoder != NULL)
			{
				decoder->memory_stream = ROMemoryStream_Create(data->file_buffer, data->file_size);

				if (decoder->memory_stream != NULL)
				{
					if (FLAC__stream_decoder_init_stream(decoder->flac_stream_decoder, MemoryFile_fread_wrapper, MemoryFile_fseek_wrapper, MemoryFile_ftell_wrapper, MemoryFile_GetSize, MemoryFile_EOF, WriteCallback, MetadataCallback, ErrorCallback, decoder) == FLAC__STREAM_DECODER_INIT_STATUS_OK)
					{
						decoder->data = data;
						decoder->error = false;
						decoder->info = info;
						decoder->loops = loops;
						FLAC__stream_decoder_process_until_end_of_metadata(decoder->flac_stream_decoder);

						if (decoder->error)
						{
							FLAC__stream_decoder_finish(decoder->flac_stream_decoder);
							FLAC__stream_decoder_delete(decoder->flac_stream_decoder);
							ROMemoryStream_Destroy(decoder->memory_stream);
							free(decoder);
							decoder = NULL;
						}

					}
					else
					{
						FLAC__stream_decoder_delete(decoder->flac_stream_decoder);
						ROMemoryStream_Destroy(decoder->memory_stream);
						free(decoder);
						decoder = NULL;
					}
				}
				else
				{
					FLAC__stream_decoder_delete(decoder->flac_stream_decoder);
					free(decoder);
					decoder = NULL;
				}
			}
			else
			{
				free(decoder);
				decoder = NULL;
			}
		}
	}

	return decoder;
}

void Decoder_libFLAC_Destroy(Decoder_libFLAC *decoder)
{
	if (decoder != NULL)
	{
		FLAC__stream_decoder_finish(decoder->flac_stream_decoder);
		FLAC__stream_decoder_delete(decoder->flac_stream_decoder);
		ROMemoryStream_Destroy(decoder->memory_stream);
		free(decoder->block_buffer);
		free(decoder);
	}
}

void Decoder_libFLAC_Rewind(Decoder_libFLAC *decoder)
{
	FLAC__stream_decoder_seek_absolute(decoder->flac_stream_decoder, 0);
}

unsigned long Decoder_libFLAC_GetSamples(Decoder_libFLAC *decoder, void *buffer_void, unsigned long frames_to_do)
{
	unsigned char *buffer = buffer_void;

	unsigned long frames_done_total = 0;

	while (frames_done_total != frames_to_do)
	{
		if (decoder->block_buffer_index == decoder->block_buffer_size)
		{
			FLAC__stream_decoder_process_single(decoder->flac_stream_decoder);

			if (FLAC__stream_decoder_get_state(decoder->flac_stream_decoder) == FLAC__STREAM_DECODER_END_OF_STREAM)
			{
				if (decoder->loops)
				{
					Decoder_libFLAC_Rewind(decoder);
					continue;
				}
				else
				{
					break;
				}
			}
		}

		const unsigned int SIZE_OF_FRAME = sizeof(FLAC__int32) * decoder->channel_count;

		const unsigned long block_frames_to_do = MIN(frames_to_do - frames_done_total, decoder->block_buffer_size - decoder->block_buffer_index);

		memcpy(buffer + (frames_done_total * SIZE_OF_FRAME), decoder->block_buffer + (decoder->block_buffer_index * SIZE_OF_FRAME), block_frames_to_do * SIZE_OF_FRAME);

		decoder->block_buffer_index += block_frames_to_do;
		frames_done_total += block_frames_to_do;
	}

	return frames_done_total;
}
