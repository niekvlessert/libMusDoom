/**
 * libMusDoom - Doom Music Playback Library
 * 
 * This library provides an API for playing Doom's MUS music files
 * using the original OPL2/OPL3 FM synthesis engine. It outputs
 * PCM audio data similar to libgme or libvgm.
 * 
 * Based on code from Chocolate Doom and the Nuked OPL3 emulator.
 * Original Doom source code Copyright (C) 1993-1996 id Software, Inc.
 * Chocolate Doom Copyright (C) 2005-2014 Simon Howard
 * Nuked OPL3 emulator Copyright (C) 2013-2018 Alexey Khokholov (Nuke.YKT)
 * 
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 */

#ifndef LIBMUSDOOM_H
#define LIBMUSDOOM_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Error codes returned by library functions.
 */
typedef enum {
    MUSDOOM_OK = 0,
    MUSDOOM_ERR_INVALID_PARAM = -1,
    MUSDOOM_ERR_OUT_OF_MEMORY = -2,
    MUSDOOM_ERR_INVALID_DATA = -3,
    MUSDOOM_ERR_NOT_INITIALIZED = -4,
    MUSDOOM_ERR_ALREADY_INITIALIZED = -5,
} musdoom_error_t;

/**
 * OPL chip type selection.
 */
typedef enum {
    MUSDOOM_OPL2 = 0,
    MUSDOOM_OPL3 = 1,
} musdoom_opl_type_t;

/**
 * Doom version for OPL driver emulation.
 * Different versions of Doom had slightly different OPL behavior.
 */
typedef enum {
    MUSDOOM_DOOM_1_1_666 = 0,   // Doom 1 v1.666
    MUSDOOM_DOOM_2_1_666 = 1,   // Doom 2 v1.666
    MUSDOOM_DOOM_1_9 = 2,       // Doom v1.9 (default)
} musdoom_doom_version_t;

/**
 * Configuration structure for the music emulator.
 */
typedef struct {
    int sample_rate;                // Audio sample rate in Hz (default: 44100)
    musdoom_opl_type_t opl_type;    // OPL chip type (default: OPL3)
    musdoom_doom_version_t doom_version;  // Doom version emulation (default: 1.9)
    int initial_volume;             // Initial volume 0-127 (default: 100)
} musdoom_config_t;

/**
 * Opaque handle to a music emulator instance.
 */
typedef struct musdoom_emulator musdoom_emulator_t;

/**
 * Get the library version string.
 * 
 * @return Version string in format "major.minor.patch"
 */
const char* musdoom_version(void);

/**
 * Get error description string for an error code.
 * 
 * @param error Error code from musdoom_error_t
 * @return Human-readable error description
 */
const char* musdoom_error_string(musdoom_error_t error);

/**
 * Initialize a configuration structure with default values.
 * 
 * @param config Pointer to configuration structure to initialize
 * @return MUSDOOM_OK on success, error code otherwise
 */
musdoom_error_t musdoom_config_init(musdoom_config_t* config);

/**
 * Create a new music emulator instance.
 * 
 * @param config Configuration for the emulator, or NULL for defaults
 * @return Handle to the emulator instance, or NULL on failure
 */
musdoom_emulator_t* musdoom_create(const musdoom_config_t* config);

/**
 * Destroy a music emulator instance and free all resources.
 * 
 * @param emulator Handle to the emulator instance
 */
void musdoom_destroy(musdoom_emulator_t* emulator);

/**
 * Load MUS music data into the emulator.
 * 
 * The data buffer must remain valid until musdoom_unload is called
 * or the emulator is destroyed.
 * 
 * @param emulator Handle to the emulator instance
 * @param data Pointer to MUS file data
 * @param size Size of the MUS data in bytes
 * @return MUSDOOM_OK on success, error code otherwise
 */
musdoom_error_t musdoom_load(musdoom_emulator_t* emulator, 
                              const uint8_t* data, 
                              size_t size);

/**
 * Unload the current music data from the emulator.
 * 
 * @param emulator Handle to the emulator instance
 */
void musdoom_unload(musdoom_emulator_t* emulator);

/**
 * Start playback of the loaded music.
 * 
 * @param emulator Handle to the emulator instance
 * @param looping If non-zero, the music will loop when it reaches the end
 * @return MUSDOOM_OK on success, error code otherwise
 */
musdoom_error_t musdoom_start(musdoom_emulator_t* emulator, int looping);

/**
 * Stop playback of the current music.
 * 
 * @param emulator Handle to the emulator instance
 */
void musdoom_stop(musdoom_emulator_t* emulator);

/**
 * Pause playback of the current music.
 * 
 * @param emulator Handle to the emulator instance
 */
void musdoom_pause(musdoom_emulator_t* emulator);

/**
 * Resume playback of paused music.
 * 
 * @param emulator Handle to the emulator instance
 */
void musdoom_resume(musdoom_emulator_t* emulator);

/**
 * Check if music is currently playing.
 * 
 * @param emulator Handle to the emulator instance
 * @return Non-zero if music is playing, 0 otherwise
 */
int musdoom_is_playing(musdoom_emulator_t* emulator);

/**
 * Set the music volume.
 * 
 * @param emulator Handle to the emulator instance
 * @param volume Volume level 0-127
 */
void musdoom_set_volume(musdoom_emulator_t* emulator, int volume);

/**
 * Get the current music volume.
 * 
 * @param emulator Handle to the emulator instance
 * @return Current volume level 0-127
 */
int musdoom_get_volume(musdoom_emulator_t* emulator);

/**
 * Generate audio samples.
 * 
 * This is the main function to get PCM audio data from the emulator.
 * Call this regularly to fill your audio buffer. The output is
 * stereo 16-bit signed PCM data.
 * 
 * @param emulator Handle to the emulator instance
 * @param buffer Output buffer for audio samples
 * @param num_samples Number of stereo samples to generate (each sample = 2 int16_t values)
 * @return Number of samples actually generated
 */
size_t musdoom_generate_samples(musdoom_emulator_t* emulator, 
                                 int16_t* buffer, 
                                 size_t num_samples);

/**
 * Get the current playback position in milliseconds.
 * 
 * @param emulator Handle to the emulator instance
 * @return Current position in milliseconds, or 0 if no music is loaded
 */
uint32_t musdoom_get_position_ms(musdoom_emulator_t* emulator);

/**
 * Get the total length of the current music in milliseconds.
 * 
 * @param emulator Handle to the emulator instance
 * @return Total length in milliseconds, or 0 if no music is loaded
 */
uint32_t musdoom_get_length_ms(musdoom_emulator_t* emulator);

/**
 * Seek to a position in the music.
 * 
 * Note: Seeking in FM-synthesized music is approximate and may
 * not be sample-accurate.
 * 
 * @param emulator Handle to the emulator instance
 * @param position_ms Position to seek to in milliseconds
 * @return MUSDOOM_OK on success, error code otherwise
 */
musdoom_error_t musdoom_seek_ms(musdoom_emulator_t* emulator, uint32_t position_ms);

/**
 * Load GENMIDI instrument data from a WAD file.
 * 
 * GENMIDI is the instrument definition file used by Doom for OPL synthesis.
 * This function allows loading custom instrument definitions.
 * 
 * @param emulator Handle to the emulator instance
 * @param data Pointer to GENMIDI lump data (from WAD file)
 * @param size Size of the GENMIDI data in bytes
 * @return MUSDOOM_OK on success, error code otherwise
 */
musdoom_error_t musdoom_load_genmidi(musdoom_emulator_t* emulator,
                                      const uint8_t* data,
                                      size_t size);

#ifdef __cplusplus
}
#endif

#endif /* LIBMUSDOOM_H */
