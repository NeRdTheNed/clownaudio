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

#ifndef CLOWNAUDIO_MIXER_H
#define CLOWNAUDIO_MIXER_H

#ifndef __cplusplus
#include <stdbool.h>
#endif
#include <stddef.h>

#if !defined(CLOWNAUDIO_EXPORT) && !defined(CLOWNAUDIO_NO_EXPORT)
#include "clownaudio_export.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif


typedef struct ClownAudio_Mixer ClownAudio_Mixer;
typedef struct ClownAudio_Sound ClownAudio_Sound;
typedef struct ClownAudio_SoundData ClownAudio_SoundData;
typedef unsigned int ClownAudio_SoundID;

typedef struct ClownAudio_SoundDataConfig
{
	// To 'predecode' means to decode sound data to raw PCM when it is loaded. This removes the overhead of decoding the sound data during playback.
	// The downside of this is that it causes sound data to take significantly longer to load. This may cause the thread you are loading the data on to stall.
	// If you use predecoding and load sound data on the main thread, then it may be best to load your sound data during a loading screen.
	/// If true, the sound *may* be predecoded if possible. If not, the sound will still be loaded, albeit not predecoded.
	bool predecode;
	/// If true, the sound *must* be predecoded if possible. If not, the function will fail.
	bool must_predecode;
	/// If sound is predecoded, then this needs to be true for `ClownAudio_SetSoundSampleRate` to work
	bool dynamic_sample_rate;
} ClownAudio_SoundDataConfig;

typedef struct ClownAudio_SoundConfig
{
	/// If true, the sound will loop indefinitely
	bool loop;
	/// If true, the sound will not be automatically destroyed once it finishes playing
	bool do_not_destroy_when_done;
	/// If sound is not predecoded, then this needs to be true for `ClownAudio_SetSoundSampleRate` to work
	bool dynamic_sample_rate;
} ClownAudio_SoundConfig;


//////////////////////////////////
// Configuration initialisation //
//////////////////////////////////

/// Initialises a `ClownAudio_SoundDataConfig` struct with sane default values
CLOWNAUDIO_EXPORT void ClownAudio_InitSoundDataConfig(ClownAudio_SoundDataConfig *config);

/// Initialises a `ClownAudio_SoundConfig` struct with sane default values
CLOWNAUDIO_EXPORT void ClownAudio_InitSoundConfig(ClownAudio_SoundConfig *config);


////////////////////////////////
// Mixer creation/destruction //
////////////////////////////////

/// Creates a mixer. Will return NULL if it fails.
CLOWNAUDIO_EXPORT ClownAudio_Mixer* ClownAudio_Mixer_Create(unsigned long sample_rate);

/// Destroys a mixer. All sounds playing through the specified mixer must be destroyed manually before this function is called.
CLOWNAUDIO_EXPORT void ClownAudio_Mixer_Destroy(ClownAudio_Mixer *mixer);


//////////////////////////////////
// Sound-data loading/unloading //
//////////////////////////////////

/// Loads data from up to two memory buffers - either buffer pointer can be NULL.
/// If two buffers are specified and looping is enabled, the sound will loop at the point where the first buffer ends and the second one begins.
CLOWNAUDIO_EXPORT ClownAudio_SoundData* ClownAudio_Mixer_SoundData_LoadFromMemory(ClownAudio_Mixer *mixer, const unsigned char *file_buffer1, size_t file_size1, const unsigned char *file_buffer2, size_t file_size2, ClownAudio_SoundDataConfig *config);

/// Loads data from up to two files - either file path can be NULL.
/// If two files are specified and looping is enabled, the sound will loop at the point where the first file ends and the second one begins.
CLOWNAUDIO_EXPORT ClownAudio_SoundData* ClownAudio_Mixer_SoundData_LoadFromFiles(ClownAudio_Mixer *mixer, const char *intro_path, const char *loop_path, ClownAudio_SoundDataConfig *config);

/// Unloads data. All sounds using the specified data must be destroyed manually before this function is called.
CLOWNAUDIO_EXPORT void ClownAudio_Mixer_SoundData_Unload(ClownAudio_Mixer *mixer, ClownAudio_SoundData *sound_data);


////////////////////////////////
// Sound creation/destruction //
////////////////////////////////

/// Creates a sound from sound-data. The sound will be paused by default.
CLOWNAUDIO_EXPORT ClownAudio_Sound* ClownAudio_Mixer_Sound_Create(ClownAudio_Mixer *mixer, ClownAudio_SoundData *sound_data, ClownAudio_SoundConfig *config);

/// Used to create a sound ID from a sound. Must be done only once.
/// Must be guarded with mutex.
CLOWNAUDIO_EXPORT ClownAudio_SoundID ClownAudio_Mixer_Sound_Register(ClownAudio_Mixer *mixer, ClownAudio_Sound *sound, ClownAudio_SoundData *sound_data);

/// Destroys sound.
/// Must be guarded with mutex.
CLOWNAUDIO_EXPORT void ClownAudio_Mixer_Sound_Destroy(ClownAudio_Mixer *mixer, ClownAudio_SoundID sound_id);


/////////////////////////////
// Assorted sound controls //
/////////////////////////////

// Playback

/// Rewinds sound to the very beginning.
/// Must be guarded with mutex.
CLOWNAUDIO_EXPORT void ClownAudio_Mixer_Sound_Rewind(ClownAudio_Mixer *mixer, ClownAudio_SoundID sound_id);

/// Pauses sound.
/// Must be guarded with mutex.
CLOWNAUDIO_EXPORT void ClownAudio_Mixer_Sound_Pause(ClownAudio_Mixer *mixer, ClownAudio_SoundID sound_id);

/// Unpauses sound.
/// Must be guarded with mutex.
CLOWNAUDIO_EXPORT void ClownAudio_Mixer_Sound_Unpause(ClownAudio_Mixer *mixer, ClownAudio_SoundID sound_id);


// Fading

/// Make sound fade-out over the specified duration, measured in milliseconds.
/// If the sound is currently fading-in, this function will override it and cause the sound to fade-out from the volume it is currently at.
/// Must be guarded with mutex.
CLOWNAUDIO_EXPORT void ClownAudio_Mixer_Sound_FadeOut(ClownAudio_Mixer *mixer, ClownAudio_SoundID sound_id, unsigned int duration);

/// Make sound fade-in over the specified duration, measured in milliseconds.
/// If the sound is currently fading-out, this function will override it and cause the sound to fade-in from the volume it is currently at.
/// Must be guarded with mutex.
CLOWNAUDIO_EXPORT void ClownAudio_Mixer_Sound_FadeIn(ClownAudio_Mixer *mixer, ClownAudio_SoundID sound_id, unsigned int duration);

/// Aborts fading and instantly restores the sound to full volume.
/// If you want to smoothly-undo an in-progress fade, then use one of the above functions instead.
/// Must be guarded with mutex.
CLOWNAUDIO_EXPORT void ClownAudio_Mixer_Sound_CancelFade(ClownAudio_Mixer *mixer, ClownAudio_SoundID sound_id);


// Miscellaneous

/// Returns -1 if the sound does not exist, 0 if it is unpaused, or 1 if it is paused.
/// Must be guarded with mutex.
CLOWNAUDIO_EXPORT int ClownAudio_Mixer_Sound_GetStatus(ClownAudio_Mixer *mixer, ClownAudio_SoundID sound_id);

/// Sets stereo volume. Volume is linear and ranges from 0 (silence) to 0x100 (full volume). Exceeding 0x100 will amplify the volume.
/// Must be guarded with mutex.
CLOWNAUDIO_EXPORT void ClownAudio_Mixer_Sound_SetVolume(ClownAudio_Mixer *mixer, ClownAudio_SoundID sound_id, unsigned short volume_left, unsigned short volume_right);

/// Change whether the sound should loop or not. Only certain file formats support this (for example, Ogg Vorbis does but PxTone doesn't).
/// Must be guarded with mutex.
CLOWNAUDIO_EXPORT void ClownAudio_Mixer_Sound_SetLoop(ClownAudio_Mixer *mixer, ClownAudio_SoundID sound_id, bool loop);

/// Sets the sound's speed. Full speed is 0x10000, half-speed is 0x8000, and double-speed is 0x20000.
/// Note: the sound must have been created with `dynamic_sample_rate` enabled in the configuration struct, otherwise this function will silently fail.
/// Must be guarded with mutex.
CLOWNAUDIO_EXPORT void ClownAudio_Mixer_Sound_SetSpeed(ClownAudio_Mixer *mixer, ClownAudio_SoundID sound_id, unsigned long speed);


////////////
// Output //
////////////

/// Mix interlaced (L,R ordering) S16 PCM samples into specified S32 buffer (not clamped).
/// Must be guarded with mutex.
CLOWNAUDIO_EXPORT void ClownAudio_Mixer_MixSamples(ClownAudio_Mixer *mixer, long *output_buffer, size_t frames_to_do);

/// Output interlaced (L,R ordering) S16 PCM samples into specified S16 buffer (clamped).
/// Must be guarded with mutex.
CLOWNAUDIO_EXPORT void ClownAudio_Mixer_OutputSamples(ClownAudio_Mixer *mixer, short *output_buffer, size_t frames_to_do);


#ifdef __cplusplus
}
#endif

#endif // CLOWNAUDIO_MIXER_H
