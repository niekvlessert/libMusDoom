/**
 * libMusDoom Example Player
 * 
 * A simple command-line player that converts MUS files to WAV.
 * Usage: musdoom_player <input.mus> <genmidi.lmp> <output.wav> [duration_seconds]
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "libmusdoom.h"

// WAV file header structure
typedef struct {
    // RIFF header
    char riff[4];           // "RIFF"
    uint32_t file_size;     // File size - 8
    char wave[4];           // "WAVE"
    
    // fmt chunk
    char fmt[4];            // "fmt "
    uint32_t fmt_size;      // 16 for PCM
    uint16_t audio_format;  // 1 for PCM
    uint16_t num_channels;  // 2 for stereo
    uint32_t sample_rate;   // 44100
    uint32_t byte_rate;     // sample_rate * num_channels * bits_per_sample/8
    uint16_t block_align;   // num_channels * bits_per_sample/8
    uint16_t bits_per_sample; // 16
    
    // data chunk
    char data[4];           // "data"
    uint32_t data_size;     // Number of bytes in data
} wav_header_t;

// Write WAV header
void write_wav_header(FILE* fp, uint32_t sample_rate, uint32_t num_samples) {
    wav_header_t header;
    uint32_t data_size = num_samples * 2 * 2;  // stereo, 16-bit
    
    memcpy(header.riff, "RIFF", 4);
    header.file_size = data_size + sizeof(wav_header_t) - 8;
    memcpy(header.wave, "WAVE", 4);
    
    memcpy(header.fmt, "fmt ", 4);
    header.fmt_size = 16;
    header.audio_format = 1;  // PCM
    header.num_channels = 2;  // Stereo
    header.sample_rate = sample_rate;
    header.byte_rate = sample_rate * 2 * 2;  // 2 channels, 16-bit
    header.block_align = 4;  // 2 channels * 2 bytes
    header.bits_per_sample = 16;
    
    memcpy(header.data, "data", 4);
    header.data_size = data_size;
    
    fwrite(&header, sizeof(header), 1, fp);
}

// Read entire file into memory
uint8_t* read_file(const char* filename, size_t* size) {
    FILE* fp;
    uint8_t* data;
    size_t file_size;
    
    fp = fopen(filename, "rb");
    if (!fp) {
        fprintf(stderr, "Error: Cannot open file '%s'\n", filename);
        return NULL;
    }
    
    fseek(fp, 0, SEEK_END);
    file_size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    
    data = (uint8_t*)malloc(file_size);
    if (!data) {
        fclose(fp);
        fprintf(stderr, "Error: Out of memory\n");
        return NULL;
    }
    
    if (fread(data, 1, file_size, fp) != file_size) {
        free(data);
        fclose(fp);
        fprintf(stderr, "Error: Failed to read file\n");
        return NULL;
    }
    
    fclose(fp);
    *size = file_size;
    return data;
}

// Print usage
void print_usage(const char* program) {
    printf("libMusDoom Player v%s\n", musdoom_version());
    printf("\n");
    printf("Usage: %s <input.mus> <genmidi.lmp> <output.wav> [duration_seconds]\n", program);
    printf("\n");
    printf("Converts Doom MUS music files to WAV audio using OPL3 synthesis.\n");
    printf("\n");
    printf("Arguments:\n");
    printf("  input.mus         MUS music file (e.g., D_E1M1.lmp from Doom)\n");
    printf("  genmidi.lmp       GENMIDI instrument file from Doom WAD\n");
    printf("  output.wav        Output WAV file\n");
    printf("  duration_seconds  Optional: maximum duration in seconds (default: 180)\n");
    printf("\n");
    printf("Options:\n");
    printf("  -h, --help        Show this help message\n");
    printf("  -l, --loop N      Loop N times (default: 1)\n");
    printf("  -v, --volume N    Set volume 0-127 (default: 100)\n");
    printf("\n");
    printf("Example:\n");
    printf("  %s D_E1M1.lmp GENMIDI.lmp e1m1.wav 30\n", program);
    printf("\n");
}

int main(int argc, char* argv[]) {
    const char* input_file = NULL;
    const char* genmidi_file = NULL;
    const char* output_file = NULL;
    int loop_count = 1;
    int volume = 100;
    int max_duration_sec = 180;  // 3 minutes default
    
    // Parse arguments
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            print_usage(argv[0]);
            return 0;
        } else if (strcmp(argv[i], "-l") == 0 || strcmp(argv[i], "--loop") == 0) {
            if (i + 1 < argc) {
                loop_count = atoi(argv[++i]);
            }
        } else if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--volume") == 0) {
            if (i + 1 < argc) {
                volume = atoi(argv[++i]);
            }
        } else if (!input_file) {
            input_file = argv[i];
        } else if (!genmidi_file) {
            genmidi_file = argv[i];
        } else if (!output_file) {
            output_file = argv[i];
        } else {
            // Duration argument
            max_duration_sec = atoi(argv[i]);
        }
    }
    
    if (!input_file || !genmidi_file || !output_file) {
        print_usage(argv[0]);
        return 1;
    }
    
    printf("libMusDoom Player v%s\n", musdoom_version());
    printf("Input: %s\n", input_file);
    printf("GENMIDI: %s\n", genmidi_file);
    printf("Output: %s\n", output_file);
    
    // Read input file
    size_t mus_size;
    uint8_t* mus_data = read_file(input_file, &mus_size);
    if (!mus_data) {
        return 1;
    }
    
    printf("Read %zu bytes from input file\n", mus_size);
    
    // Read GENMIDI file
    size_t genmidi_size;
    uint8_t* genmidi_data = read_file(genmidi_file, &genmidi_size);
    if (!genmidi_data) {
        free(mus_data);
        return 1;
    }
    
    printf("Read %zu bytes from GENMIDI file\n", genmidi_size);
    
    // Create emulator
    musdoom_config_t config;
    musdoom_config_init(&config);
    config.sample_rate = 44100;
    config.opl_type = MUSDOOM_OPL3;
    config.initial_volume = volume;
    
    musdoom_emulator_t* emu = musdoom_create(&config);
    if (!emu) {
        fprintf(stderr, "Error: Failed to create emulator\n");
        free(genmidi_data);
        free(mus_data);
        return 1;
    }
    
    // Load GENMIDI instruments
    musdoom_error_t err = musdoom_load_genmidi(emu, genmidi_data, genmidi_size);
    if (err != MUSDOOM_OK) {
        fprintf(stderr, "Error: Failed to load GENMIDI: %s\n", musdoom_error_string(err));
        musdoom_destroy(emu);
        free(genmidi_data);
        free(mus_data);
        return 1;
    }
    
    printf("GENMIDI instruments loaded\n");
    
    // Load music
    err = musdoom_load(emu, mus_data, mus_size);
    if (err != MUSDOOM_OK) {
        fprintf(stderr, "Error: Failed to load music: %s\n", musdoom_error_string(err));
        musdoom_destroy(emu);
        free(genmidi_data);
        free(mus_data);
        return 1;
    }
    
    printf("Music loaded successfully\n");
    
    // Get song length
    uint32_t length_ms = musdoom_get_length_ms(emu);
    printf("Song length: %u:%02u\n", length_ms / 60000, (length_ms / 1000) % 60);
    
    // Open output file
    FILE* output = fopen(output_file, "wb");
    if (!output) {
        fprintf(stderr, "Error: Cannot create output file\n");
        musdoom_unload(emu);
        musdoom_destroy(emu);
        free(genmidi_data);
        free(mus_data);
        return 1;
    }
    
    // Write placeholder WAV header
    wav_header_t wav_header;
    memset(&wav_header, 0, sizeof(wav_header));
    fwrite(&wav_header, sizeof(wav_header), 1, output);
    
    // Generate audio
    int16_t buffer[4096];  // 2048 stereo samples
    size_t total_samples = 0;
    size_t max_samples = (size_t)max_duration_sec * 44100;
    
    printf("Rendering audio (max %d seconds)...\n", max_duration_sec);
    
    for (int loop = 0; loop < loop_count; loop++) {
        musdoom_start(emu, 0);
        
        while (musdoom_is_playing(emu) && total_samples < max_samples) {
            size_t samples_to_gen = 2048;
            if (total_samples + samples_to_gen > max_samples) {
                samples_to_gen = max_samples - total_samples;
            }
            
            size_t samples = musdoom_generate_samples(emu, buffer, samples_to_gen);
            if (samples > 0) {
                fwrite(buffer, sizeof(int16_t), samples * 2, output);
                total_samples += samples;
            }
            
            // Progress indicator
            if (total_samples % (44100 * 5) == 0) {
                printf("  %zu seconds rendered...\n", total_samples / 44100);
            }
        }
        
        printf("Loop %d/%d complete (%zu samples)\n", loop + 1, loop_count, total_samples);
    }
    
    // Update WAV header
    fseek(output, 0, SEEK_SET);
    write_wav_header(output, 44100, total_samples);
    
    fclose(output);
    
    printf("Wrote %zu samples (%.1f seconds) to %s\n", 
           total_samples, (double)total_samples / 44100.0, output_file);
    
    // Cleanup
    musdoom_unload(emu);
    musdoom_destroy(emu);
    free(genmidi_data);
    free(mus_data);
    
    return 0;
}
