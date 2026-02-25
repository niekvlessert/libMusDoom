/**
 * libMusDoom - Doom Music Playback Library Implementation
 * 
 * Library for playing Doom MUS music files using OPL3 FM synthesis
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "libmusdoom.h"
#include "doom_music.h"

// Version string
#define MUSDOOM_VERSION "1.0.0"

// Library version
const char* musdoom_version(void) {
    return MUSDOOM_VERSION;
}

// Error strings
const char* musdoom_error_string(musdoom_error_t error) {
    switch (error) {
        case MUSDOOM_OK:
            return "Success";
        case MUSDOOM_ERR_INVALID_PARAM:
            return "Invalid parameter";
        case MUSDOOM_ERR_OUT_OF_MEMORY:
            return "Out of memory";
        case MUSDOOM_ERR_INVALID_DATA:
            return "Invalid data";
        case MUSDOOM_ERR_NOT_INITIALIZED:
            return "Not initialized";
        case MUSDOOM_ERR_ALREADY_INITIALIZED:
            return "Already initialized";
        default:
            return "Unknown error";
    }
}

// Initialize configuration with defaults
musdoom_error_t musdoom_config_init(musdoom_config_t* config) {
    if (!config) {
        return MUSDOOM_ERR_INVALID_PARAM;
    }
    
    config->sample_rate = 44100;
    config->opl_type = MUSDOOM_OPL3;
    config->doom_version = MUSDOOM_DOOM_1_9;
    config->initial_volume = 100;
    
    return MUSDOOM_OK;
}

// Create emulator instance
musdoom_emulator_t* musdoom_create(const musdoom_config_t* config) {
    musdoom_emulator_t* emu;
    musdoom_config_t default_config;
    
    if (!config) {
        musdoom_config_init(&default_config);
        config = &default_config;
    }
    
    emu = (musdoom_emulator_t*)calloc(1, sizeof(musdoom_emulator_t));
    if (!emu) {
        return NULL;
    }
    
    emu->sample_rate = config->sample_rate;
    emu->opl3_mode = config->opl_type == MUSDOOM_OPL3;
    emu->driver_version = (opl_driver_ver_t)config->doom_version;
    emu->current_volume = config->initial_volume;
    emu->initial_volume = config->initial_volume;
    
    // Create MUS player
    emu->mus_player = mus_player_create(config->sample_rate);
    if (!emu->mus_player) {
        free(emu);
        return NULL;
    }

    mus_player_set_driver_version(emu->mus_player, emu->driver_version);
    mus_player_set_opl3_mode(emu->mus_player, emu->opl3_mode);
    mus_player_set_master_volume(emu->mus_player, emu->current_volume);
    
    return emu;
}

// Destroy emulator
void musdoom_destroy(musdoom_emulator_t* emu) {
    if (!emu) return;
    
    musdoom_stop(emu);
    musdoom_unload(emu);
    
    if (emu->mus_player) {
        mus_player_destroy(emu->mus_player);
    }
    
    free(emu->main_instrs);
    free(emu->perc_instrs);
    free(emu);
}

// Load music data
musdoom_error_t musdoom_load(musdoom_emulator_t* emu, const uint8_t* data, size_t size) {
    if (!emu || !data || size == 0) {
        return MUSDOOM_ERR_INVALID_PARAM;
    }
    
    // Unload any existing music
    musdoom_unload(emu);
    
    // Load into MUS player
    if (mus_player_load(emu->mus_player, data, size) != 0) {
        return MUSDOOM_ERR_INVALID_DATA;
    }
    
    // Store the data reference
    emu->music_data = data;
    emu->music_size = size;
    
    return MUSDOOM_OK;
}

// Unload music
void musdoom_unload(musdoom_emulator_t* emu) {
    if (!emu) return;
    
    if (emu->mus_player) {
        mus_player_stop(emu->mus_player);
    }
    
    emu->music_data = NULL;
    emu->music_size = 0;
}

// Start playback
musdoom_error_t musdoom_start(musdoom_emulator_t* emu, int looping) {
    if (!emu || !emu->music_data) {
        return MUSDOOM_ERR_INVALID_PARAM;
    }
    
    mus_player_start(emu->mus_player, looping);
    
    emu->looping = looping;
    emu->playing = 1;
    emu->paused = 0;
    emu->current_time_us = 0;
    emu->start_volume = emu->current_volume;
    
    return MUSDOOM_OK;
}

// Stop playback
void musdoom_stop(musdoom_emulator_t* emu) {
    if (!emu) return;
    
    if (emu->mus_player) {
        mus_player_stop(emu->mus_player);
    }
    
    emu->playing = 0;
}

// Pause playback
void musdoom_pause(musdoom_emulator_t* emu) {
    if (!emu) return;
    emu->paused = 1;
}

// Resume playback
void musdoom_resume(musdoom_emulator_t* emu) {
    if (!emu) return;
    emu->paused = 0;
}

// Check if playing
int musdoom_is_playing(musdoom_emulator_t* emu) {
    if (!emu) return 0;
    return emu->playing && !emu->paused;
}

// Set volume
void musdoom_set_volume(musdoom_emulator_t* emu, int volume) {
    if (!emu) return;
    if (volume < 0) volume = 0;
    if (volume > 127) volume = 127;
    emu->current_volume = volume;
    if (emu->mus_player) {
        mus_player_set_master_volume(emu->mus_player, volume);
    }
}

// Get volume
int musdoom_get_volume(musdoom_emulator_t* emu) {
    if (!emu) return 0;
    return emu->current_volume;
}

// Generate samples
size_t musdoom_generate_samples(musdoom_emulator_t* emu, int16_t* buffer, size_t num_samples) {
    if (!emu || !buffer || num_samples == 0) {
        return 0;
    }
    
    if (!emu->playing || emu->paused) {
        // Generate silence
        memset(buffer, 0, num_samples * 2 * sizeof(int16_t));
        return num_samples;
    }
    
    // Generate samples from MUS player
    size_t generated = mus_player_generate(emu->mus_player, buffer, num_samples);
    
    // Update time
    if (generated > 0) {
        emu->current_time_us = mus_player_get_position_ms(emu->mus_player) * 1000ULL;
    }
    
    // Check if playback ended
    if (!mus_player_is_playing(emu->mus_player)) {
        emu->playing = 0;
    }
    
    return generated;
}

// Get position in milliseconds
uint32_t musdoom_get_position_ms(musdoom_emulator_t* emu) {
    if (!emu) return 0;
    return (uint32_t)(emu->current_time_us / 1000);
}

// Get length in milliseconds
uint32_t musdoom_get_length_ms(musdoom_emulator_t* emu) {
    if (!emu) return 0;
    // Approximate length - would need to parse entire file for exact length
    return 180000;  // 3 minutes default
}

// Seek to position
musdoom_error_t musdoom_seek_ms(musdoom_emulator_t* emu, uint32_t position_ms) {
    if (!emu) {
        return MUSDOOM_ERR_INVALID_PARAM;
    }
    
    // For now, just restart from beginning
    // Full implementation would need to parse events to seek
    musdoom_stop(emu);
    musdoom_start(emu, emu->looping);
    
    return MUSDOOM_OK;
}

// Load GENMIDI instruments
musdoom_error_t musdoom_load_genmidi(musdoom_emulator_t* emu, const uint8_t* data, size_t size) {
    if (!emu || !data || size < 8) {
        return MUSDOOM_ERR_INVALID_PARAM;
    }
    
    // Load into MUS player
    if (mus_player_load_instruments(emu->mus_player, data, size) != 0) {
        return MUSDOOM_ERR_INVALID_DATA;
    }
    
    emu->instruments_loaded = 1;
    
    return MUSDOOM_OK;
}
