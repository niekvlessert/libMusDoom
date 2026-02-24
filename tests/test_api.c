/**
 * API Test for libMusDoom
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "libmusdoom.h"

void test_version(void) {
    printf("Testing version... ");
    const char* version = musdoom_version();
    assert(version != NULL);
    assert(strlen(version) > 0);
    printf("OK (%s)\n", version);
}

void test_error_strings(void) {
    printf("Testing error strings... ");
    assert(strcmp(musdoom_error_string(MUSDOOM_OK), "Success") == 0);
    assert(strcmp(musdoom_error_string(MUSDOOM_ERR_INVALID_PARAM), "Invalid parameter") == 0);
    assert(strcmp(musdoom_error_string(MUSDOOM_ERR_OUT_OF_MEMORY), "Out of memory") == 0);
    assert(strcmp(musdoom_error_string(MUSDOOM_ERR_INVALID_DATA), "Invalid data") == 0);
    printf("OK\n");
}

void test_config(void) {
    printf("Testing configuration... ");
    
    musdoom_config_t config;
    musdoom_error_t err = musdoom_config_init(&config);
    assert(err == MUSDOOM_OK);
    assert(config.sample_rate == 44100);
    assert(config.opl_type == MUSDOOM_OPL3);
    assert(config.doom_version == MUSDOOM_DOOM_1_9);
    assert(config.initial_volume == 100);
    
    // Test with NULL
    err = musdoom_config_init(NULL);
    assert(err == MUSDOOM_ERR_INVALID_PARAM);
    
    printf("OK\n");
}

void test_create_destroy(void) {
    printf("Testing create/destroy... ");
    
    musdoom_emulator_t* emu = musdoom_create(NULL);
    assert(emu != NULL);
    
    musdoom_destroy(emu);
    
    // Test with custom config
    musdoom_config_t config;
    musdoom_config_init(&config);
    config.sample_rate = 22050;
    config.opl_type = MUSDOOM_OPL2;
    
    emu = musdoom_create(&config);
    assert(emu != NULL);
    musdoom_destroy(emu);
    
    printf("OK\n");
}

void test_volume(void) {
    printf("Testing volume... ");
    
    musdoom_emulator_t* emu = musdoom_create(NULL);
    assert(emu != NULL);
    
    // Default volume
    int vol = musdoom_get_volume(emu);
    assert(vol == 100);
    
    // Set volume
    musdoom_set_volume(emu, 50);
    vol = musdoom_get_volume(emu);
    assert(vol == 50);
    
    // Boundary tests
    musdoom_set_volume(emu, 0);
    assert(musdoom_get_volume(emu) == 0);
    
    musdoom_set_volume(emu, 127);
    assert(musdoom_get_volume(emu) == 127);
    
    musdoom_set_volume(emu, 200);  // Should clamp to 127
    assert(musdoom_get_volume(emu) == 127);
    
    musdoom_destroy(emu);
    printf("OK\n");
}

void test_generate_samples(void) {
    printf("Testing sample generation... ");
    
    musdoom_emulator_t* emu = musdoom_create(NULL);
    assert(emu != NULL);
    
    int16_t buffer[1024];
    size_t samples = musdoom_generate_samples(emu, buffer, 512);
    assert(samples == 512);
    
    // Test with NULL
    samples = musdoom_generate_samples(emu, NULL, 512);
    assert(samples == 0);
    
    samples = musdoom_generate_samples(NULL, buffer, 512);
    assert(samples == 0);
    
    musdoom_destroy(emu);
    printf("OK\n");
}

void test_playback_controls(void) {
    printf("Testing playback controls... ");
    
    musdoom_emulator_t* emu = musdoom_create(NULL);
    assert(emu != NULL);
    
    // Initially not playing
    assert(musdoom_is_playing(emu) == 0);
    
    // Pause/resume without music
    musdoom_pause(emu);
    musdoom_resume(emu);
    
    // Stop without music
    musdoom_stop(emu);
    
    musdoom_destroy(emu);
    printf("OK\n");
}

void test_invalid_load(void) {
    printf("Testing invalid load... ");
    
    musdoom_emulator_t* emu = musdoom_create(NULL);
    assert(emu != NULL);
    
    // NULL data
    musdoom_error_t err = musdoom_load(emu, NULL, 100);
    assert(err == MUSDOOM_ERR_INVALID_PARAM);
    
    // Zero size
    uint8_t dummy[10] = {0};
    err = musdoom_load(emu, dummy, 0);
    assert(err == MUSDOOM_ERR_INVALID_PARAM);
    
    // Invalid data
    err = musdoom_load(emu, dummy, 10);
    assert(err == MUSDOOM_ERR_INVALID_DATA);
    
    musdoom_destroy(emu);
    printf("OK\n");
}

int main(void) {
    printf("=== libMusDoom API Tests ===\n\n");
    
    test_version();
    test_error_strings();
    test_config();
    test_create_destroy();
    test_volume();
    test_generate_samples();
    test_playback_controls();
    test_invalid_load();
    
    printf("\n=== All tests passed! ===\n");
    return 0;
}
