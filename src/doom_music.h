/**
 * Doom Music Emulator - Internal Header
 * 
 * Combines OPL3 synthesis with MUS/MIDI file playback
 */

#ifndef DOOM_MUSIC_H
#define DOOM_MUSIC_H

#include <stdint.h>
#include "internal/types.h"
#include "opl3.h"

// GENMIDI instrument definitions
#define GENMIDI_NUM_INSTRS 175
#define GENMIDI_NUM_PERCUSSION 47
#define GENMIDI_HEADER "#OPL_II#"
#define GENMIDI_FLAG_FIXED 0x0001
#define GENMIDI_FLAG_2VOICE 0x0004

// OPL voice count
#define OPL_VOICES 18  // 9 voices * 2 for OPL3
#define OPL_NUM_OPERATORS 36
#define OPL_NUM_VOICES 18

// MIDI channels
#define MIDI_CHANNELS_PER_TRACK 16

// GENMIDI operator structure (from Chocolate Doom)
typedef struct {
    Bit8u tremolo;       // Tremolo/vibrato/sustain/KSR/multiplier
    Bit8u attack;        // Attack rate/decay rate
    Bit8u sustain;       // Sustain level/release rate
    Bit8u waveform;      // Waveform select
    Bit8u scale;         // Key scale level
    Bit8u level;         // Output level
} genmidi_op_t;

// GENMIDI voice structure (from Chocolate Doom)
typedef struct {
    genmidi_op_t modulator;   // Modulator operator
    Bit8u feedback;           // Feedback/connection
    genmidi_op_t carrier;     // Carrier operator
    Bit8u unused;
    Bit16s base_note_offset;  // Note offset for tuning
} genmidi_voice_t;

// GENMIDI instrument structure (from Chocolate Doom)
typedef struct {
    Bit16u flags;             // Instrument flags
    Bit8u fine_tuning;        // Fine tuning
    Bit8u fixed_note;         // Fixed note number
    genmidi_voice_t voices[2]; // Two voices for double-voice instruments
} genmidi_instr_t;

// OPL driver version
typedef enum {
    opl_doom1_1_666,
    opl_doom2_1_666,
    opl_doom_1_9
} opl_driver_ver_t;

// MUS player forward declaration
typedef struct mus_player_s mus_player_t;

// MUS player functions
mus_player_t* mus_player_create(int sample_rate);
void mus_player_destroy(mus_player_t* player);
int mus_player_load(mus_player_t* player, const uint8_t* data, size_t size);
int mus_player_load_instruments(mus_player_t* player, const uint8_t* data, size_t size);
void mus_player_start(mus_player_t* player, int looping);
void mus_player_stop(mus_player_t* player);
int mus_player_is_playing(mus_player_t* player);
size_t mus_player_generate(mus_player_t* player, int16_t* buffer, size_t num_samples);
uint32_t mus_player_get_position_ms(mus_player_t* player);
void mus_player_set_master_volume(mus_player_t* player, int volume);
void mus_player_set_driver_version(mus_player_t* player, opl_driver_ver_t version);
void mus_player_set_opl3_mode(mus_player_t* player, int opl3_mode);

// Channel data
typedef struct {
    genmidi_instr_t *instrument;
    int volume;
    int volume_base;
    int pan;
    int bend;
} opl_channel_data_t;

// Voice structure
typedef struct opl_voice_s {
    int index;
    int op1, op2;
    int array;
    genmidi_instr_t *current_instr;
    unsigned int current_instr_voice;
    opl_channel_data_t *channel;
    unsigned int key;
    unsigned int note;
    unsigned int freq;
    unsigned int note_volume;
    unsigned int car_volume;
    unsigned int mod_volume;
    unsigned int reg_pan;
    unsigned int priority;
} opl_voice_t;

// Main emulator structure
struct musdoom_emulator {
    // Configuration
    int sample_rate;
    int opl3_mode;
    opl_driver_ver_t driver_version;
    int initial_volume;
    
    // OPL3 chip
    opl3_chip opl_chip;
    
    // Instruments
    genmidi_instr_t *main_instrs;
    genmidi_instr_t *perc_instrs;
    int instruments_loaded;
    
    // MUS player
    mus_player_t *mus_player;
    
    // Voices
    opl_voice_t voices[OPL_VOICES];
    opl_voice_t *voice_free_list[OPL_VOICES];
    opl_voice_t *voice_alloced_list[OPL_VOICES];
    int voice_free_num;
    int voice_alloced_num;
    
    // Channels
    opl_channel_data_t channels[MIDI_CHANNELS_PER_TRACK];
    
    // Playback state
    int playing;
    int looping;
    int paused;
    int current_volume;
    int start_volume;
    
    // Timing
    uint64_t current_time_us;
    uint64_t song_length_us;
    
    // Music data
    const uint8_t *music_data;
    size_t music_size;
};

// Volume table
extern const unsigned int volume_mapping[128];

#endif /* DOOM_MUSIC_H */
