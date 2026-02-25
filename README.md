# libMusDoom

A C library for playing Doom MUS music files using OPL3 FM synthesis, outputting PCM audio data similar to [libgme](https://bitbucket.org/mpyne/game-music-emu/wiki/Home) or [libvgm](https://github.com/ValleyBell/libvgm).

This library faithfully reproduces the sound of the original Doom (1993) music by using the same OPL2/OPL3 FM synthesis engine that was used in the DOS version of the game.

## Features

- Authentic Doom music playback using OPL3 FM synthesis
- Simple C API similar to libgme/libvgm
- Stereo 16-bit PCM audio output
- Configurable sample rate
- Support for custom GENMIDI instrument definitions
- Looping support
- Volume control

## Building

### Prerequisites

- CMake 3.10 or higher
- C compiler (GCC, Clang, or MSVC)

### Compile

```bash
mkdir build
cd build
cmake ..
make
```

### Build musdoom_player (WAV generator)

The `musdoom_player` example renders a MUS file to a WAV file.

```bash
cmake -S . -B build
cmake --build build --target musdoom_player
```

### Install

```bash
sudo make install
```

## API Overview

```c
#include <musdoom.h>

// Create emulator
musdoom_config_t config;
musdoom_config_init(&config);
config.sample_rate = 44100;

musdoom_emulator_t* emu = musdoom_create(&config);

// Load GENMIDI instruments (required)
musdoom_load_genmidi(emu, genmidi_data, genmidi_size);

// Load MUS file
musdoom_load(emu, mus_data, mus_size);

// Start playback
musdoom_start(emu, 1);  // 1 = looping

// Generate audio samples
int16_t buffer[4096];
size_t samples = musdoom_generate_samples(emu, buffer, 2048);

// Cleanup
musdoom_destroy(emu);
```

## musdoom_player Usage (WAV Generator)

`musdoom_player` converts MUS files to WAV using the same synthesis engine as the library.

```bash
./build/examples/musdoom_player <input.mus> <genmidi.lmp> <output.wav> [duration_seconds]
```

Examples:

```bash
# Render 30 seconds of E1M1
./build/examples/musdoom_player testdata/D_E1M1.lmp testdata/GENMIDI.lmp e1m1.wav 30

# Render with looping (2 loops) and cap total output at 60 seconds
./build/examples/musdoom_player -l 2 testdata/D_E1M1.lmp testdata/GENMIDI.lmp e1m1_loop.wav 60
```

Options:

- `-l, --loop N` Loop N times using internal MUS looping
- `-v, --volume N` Volume 0-127 (default: 100)

## Example: Integrating into an Audio Player

Here's a complete example showing how to integrate libMusDoom into your audio player project:

```c
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "libmusdoom.h"

// Audio callback - call this from your audio system
void audio_callback(void* userdata, int16_t* buffer, size_t num_samples) {
    musdoom_emulator_t* emu = (musdoom_emulator_t*)userdata;
    musdoom_generate_samples(emu, buffer, num_samples);
}

// Load and play a MUS file
int play_mus_file(const char* mus_path, 
                  const uint8_t* genmidi_data, 
                  size_t genmidi_size) {
    
    // Read MUS file
    FILE* fp = fopen(mus_path, "rb");
    if (!fp) return -1;
    
    fseek(fp, 0, SEEK_END);
    size_t mus_size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    
    uint8_t* mus_data = malloc(mus_size);
    fread(mus_data, 1, mus_size, fp);
    fclose(fp);
    
    // Create emulator with default settings
    musdoom_config_t config;
    musdoom_config_init(&config);
    config.sample_rate = 44100;      // CD quality
    config.opl_type = MUSDOOM_OPL3;  // OPL3 for best quality
    config.initial_volume = 100;     // Volume 0-127
    
    musdoom_emulator_t* emu = musdoom_create(&config);
    if (!emu) {
        free(mus_data);
        return -1;
    }
    
    // Load GENMIDI instruments (required for correct sound)
    if (musdoom_load_genmidi(emu, genmidi_data, genmidi_size) != MUSDOOM_OK) {
        musdoom_destroy(emu);
        free(mus_data);
        return -1;
    }
    
    // Load the MUS music
    if (musdoom_load(emu, mus_data, mus_size) != MUSDOOM_OK) {
        musdoom_destroy(emu);
        free(mus_data);
        return -1;
    }
    
    // Start playback with looping enabled
    musdoom_start(emu, 1);
    
    // Now connect to your audio system
    // For example, with SDL_Audio, PortAudio, etc.
    // audio_system_set_callback(audio_callback, emu);
    
    // ... your audio playback loop here ...
    
    // Cleanup when done
    musdoom_stop(emu);
    musdoom_unload(emu);
    musdoom_destroy(emu);
    free(mus_data);
    
    return 0;
}

// Playback control functions
void pause_music(musdoom_emulator_t* emu) {
    musdoom_pause(emu);
}

void resume_music(musdoom_emulator_t* emu) {
    musdoom_resume(emu);
}

void set_volume(musdoom_emulator_t* emu, int volume) {
    // Volume range: 0-127
    musdoom_set_volume(emu, volume);
}

int is_playing(musdoom_emulator_t* emu) {
    return musdoom_is_playing(emu);
}

uint32_t get_position_ms(musdoom_emulator_t* emu) {
    return musdoom_get_position_ms(emu);
}
```

## CMake Integration

To use libMusDoom in your CMake project:

```cmake
# Find libMusDoom
find_package(musdoom REQUIRED)

# Link to your target
target_link_libraries(your_target PRIVATE musdoom::musdoom)
```

Or if using pkg-config:

```cmake
find_package(PkgConfig REQUIRED)
pkg_check_modules(MUSDOOM REQUIRED musdoom)

target_include_directories(your_target PRIVATE ${MUSDOOM_INCLUDE_DIRS})
target_link_libraries(your_target PRIVATE ${MUSDOOM_LIBRARIES})
```

## GENMIDI File

The GENMIDI file contains the OPL instrument definitions used by Doom. You can extract it from the original Doom WAD files using the included `wadextract` tool:

```bash
./wadextract doom.wad GENMIDI
```

Or use the `GENMIDI.lmp` file included in the main directory.

## MUS File Format

MUS is the music format used by Doom. It's a simplified MIDI-like format. MUS files can be extracted from Doom WAD files:

```bash
./wadextract doom.wad D_E1M1
```

## API Reference

### Version and Errors

| Function | Description |
|----------|-------------|
| `musdoom_version()` | Get library version string |
| `musdoom_error_string(error)` | Get human-readable error description |

### Emulator Lifecycle

| Function | Description |
|----------|-------------|
| `musdoom_config_init(config)` | Initialize config with defaults |
| `musdoom_create(config)` | Create emulator instance |
| `musdoom_destroy(emu)` | Destroy emulator and free resources |

### Music Loading

| Function | Description |
|----------|-------------|
| `musdoom_load(emu, data, size)` | Load MUS music data |
| `musdoom_unload(emu)` | Unload current music |
| `musdoom_load_genmidi(emu, data, size)` | Load instrument definitions |

### Playback Control

| Function | Description |
|----------|-------------|
| `musdoom_start(emu, looping)` | Start playback |
| `musdoom_stop(emu)` | Stop playback |
| `musdoom_pause(emu)` | Pause playback |
| `musdoom_resume(emu)` | Resume playback |
| `musdoom_is_playing(emu)` | Check if playing |

### Audio Generation

| Function | Description |
|----------|-------------|
| `musdoom_generate_samples(emu, buffer, num_samples)` | Generate PCM audio samples |

### Volume and Position

| Function | Description |
|----------|-------------|
| `musdoom_set_volume(emu, volume)` | Set volume (0-127) |
| `musdoom_get_volume(emu)` | Get current volume |
| `musdoom_get_position_ms(emu)` | Get playback position |
| `musdoom_get_length_ms(emu)` | Get total length |
| `musdoom_seek_ms(emu, position)` | Seek to position |

## Configuration Options

```c
typedef struct {
    int sample_rate;                  // Audio sample rate (default: 44100)
    musdoom_opl_type_t opl_type;      // OPL2 or OPL3 (default: OPL3)
    musdoom_doom_version_t doom_version;  // Doom version (default: 1.9)
    int initial_volume;               // Volume 0-127 (default: 100)
} musdoom_config_t;
```

### OPL Types

- `MUSDOOM_OPL2` - Original OPL2 chip (used in older Sound Blaster cards)
- `MUSDOOM_OPL3` - OPL3 chip (better quality, default)

### Doom Versions

Different versions of Doom had slightly different OPL driver behavior:

- `MUSDOOM_DOOM_1_1_666` - Doom 1 v1.666
- `MUSDOOM_DOOM_2_1_666` - Doom 2 v1.666  
- `MUSDOOM_DOOM_1_9` - Doom v1.9 (default)

## License

GNU General Public License v2.0 or later.

Based on code from:
- Chocolate Doom (Copyright © 2005-2014 Simon Howard)
- Nuked OPL3 emulator (Copyright © 2013-2018 Alexey Khokholov)
- Original Doom source code (Copyright © 1993-1996 id Software, Inc.)

## Credits

- id Software for the original Doom and its music system
- Simon Howard for Chocolate Doom
- Alexey Khokholov (Nuke.YKT) for the Nuked OPL3 emulator
