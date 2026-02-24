/**
 * Simple test program for libMusDoom
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "libmusdoom.h"

int main(int argc, char* argv[]) {
    printf("libMusDoom Test Program v%s\n", musdoom_version());
    
    // Test configuration
    musdoom_config_t config;
    musdoom_error_t err = musdoom_config_init(&config);
    if (err != MUSDOOM_OK) {
        printf("Failed to init config: %s\n", musdoom_error_string(err));
        return 1;
    }
    
    printf("Default config:\n");
    printf("  Sample rate: %d\n", config.sample_rate);
    printf("  OPL type: %s\n", config.opl_type == MUSDOOM_OPL3 ? "OPL3" : "OPL2");
    printf("  Doom version: %d\n", config.doom_version);
    printf("  Initial volume: %d\n", config.initial_volume);
    
    // Create emulator
    musdoom_emulator_t* emu = musdoom_create(&config);
    if (!emu) {
        printf("Failed to create emulator\n");
        return 1;
    }
    
    printf("Emulator created successfully\n");
    
    // Test volume
    musdoom_set_volume(emu, 80);
    printf("Volume set to: %d\n", musdoom_get_volume(emu));
    
    // Generate some silence
    int16_t buffer[1024];
    size_t samples = musdoom_generate_samples(emu, buffer, 512);
    printf("Generated %zu samples of silence\n", samples);
    
    // Cleanup
    musdoom_destroy(emu);
    printf("Emulator destroyed\n");
    
    printf("\nAll tests passed!\n");
    return 0;
}
