/**
 * MUS Player - Direct MUS file parser and OPL3 synthesizer
 * 
 * Based on the MUS format from id Software's Doom
 * Original MUS format designed by Paul Radek
 * 
 * Uses OPL3 register programming from Chocolate Doom
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "doom_music.h"
#include "opl3.h"

// MUS file header
typedef struct {
    uint8_t id[4];           // "MUS\x1a"
    uint16_t score_len;      // Length of score in bytes
    uint16_t score_start;    // Offset to score data
    uint16_t channels;       // Primary channels
    uint16_t sec_channels;   // Secondary channels
    uint16_t instr_count;    // Number of instruments
} mus_header_t;

// MUS event types
#define MUS_EVENT_RELEASE_NOTE    0x00
#define MUS_EVENT_PLAY_NOTE       0x10
#define MUS_EVENT_PITCH_BEND      0x20
#define MUS_EVENT_SYSTEM_EVENT    0x30
#define MUS_EVENT_CONTROLLER      0x40
#define MUS_EVENT_END_OF_SCORE    0x60

// OPL register base addresses (from Chocolate Doom)
#define OPL_REGS_TREMOLO      0x20
#define OPL_REGS_LEVEL        0x40
#define OPL_REGS_ATTACK       0x60
#define OPL_REGS_SUSTAIN      0x80
#define OPL_REGS_FEEDBACK     0xC0
#define OPL_REGS_WAVEFORM     0xE0
#define OPL_REGS_FREQ_1       0xA0
#define OPL_REGS_FREQ_2       0xB0

// GENMIDI flags
#define GENMIDI_FLAG_FIXED    0x0001
#define GENMIDI_FLAG_2VOICE   0x0004

// MUS controller to MIDI controller mapping
static const uint8_t mus_to_midi_ctrl[] = {
    0,   // 0: Program change
    0,   // 1: Bank select
    1,   // 2: Modulation
    7,   // 3: Volume
    10,  // 4: Pan
    11,  // 5: Expression
    91,  // 6: Reverb depth
    93,  // 7: Chorus depth
    64,  // 8: Sustain pedal
    67,  // 9: Soft pedal
    120, // 10: All sounds off
    123, // 11: All notes off
    126, // 12: Mono
    127, // 13: Poly
    121, // 14: Reset all controllers
    0,   // 15: Not used
};

// Operators for OPL3 voices
static const int voice_operators[2][9] = {
    { 0x00, 0x01, 0x02, 0x08, 0x09, 0x0a, 0x10, 0x11, 0x12 },
    { 0x03, 0x04, 0x05, 0x0b, 0x0c, 0x0d, 0x13, 0x14, 0x15 }
};

// Volume mapping table (from Chocolate Doom) - exact copy
static const unsigned int volume_mapping_table[128] = {
    0, 1, 3, 5, 6, 8, 10, 11,
    13, 14, 16, 17, 19, 20, 22, 23,
    25, 26, 27, 29, 30, 32, 33, 34,
    36, 37, 39, 41, 43, 45, 47, 49,
    50, 52, 54, 55, 57, 59, 60, 61,
    63, 64, 66, 67, 68, 69, 71, 72,
    73, 74, 75, 76, 77, 79, 80, 81,
    82, 83, 84, 84, 85, 86, 87, 88,
    89, 90, 91, 92, 92, 93, 94, 95,
    96, 96, 97, 98, 99, 99, 100, 101,
    101, 102, 103, 103, 104, 105, 105, 106,
    107, 107, 108, 109, 109, 110, 110, 111,
    112, 112, 113, 113, 114, 114, 115, 115,
    116, 117, 117, 118, 118, 119, 119, 120,
    120, 121, 121, 122, 122, 123, 123, 123,
    124, 124, 125, 125, 126, 126, 127, 127
};

// Frequency curve table (from Chocolate Doom)
static const unsigned short frequency_curve[] = {
    0x133, 0x133, 0x134, 0x134, 0x135, 0x136, 0x136, 0x137,
    0x137, 0x138, 0x138, 0x139, 0x139, 0x13a, 0x13b, 0x13b,
    0x13c, 0x13c, 0x13d, 0x13d, 0x13e, 0x13f, 0x13f, 0x140,
    0x140, 0x141, 0x142, 0x142, 0x143, 0x143, 0x144, 0x144,

    0x145, 0x146, 0x146, 0x147, 0x147, 0x148, 0x149, 0x149,
    0x14a, 0x14a, 0x14b, 0x14c, 0x14c, 0x14d, 0x14d, 0x14e,
    0x14f, 0x14f, 0x150, 0x150, 0x151, 0x152, 0x152, 0x153,
    0x153, 0x154, 0x155, 0x155, 0x156, 0x157, 0x157, 0x158,

    // These are used for the first seven MIDI note values:
    0x158, 0x159, 0x15a, 0x15a, 0x15b, 0x15b, 0x15c, 0x15d,
    0x15d, 0x15e, 0x15f, 0x15f, 0x160, 0x161, 0x161, 0x162,
    0x162, 0x163, 0x164, 0x164, 0x165, 0x166, 0x166, 0x167,
    0x168, 0x168, 0x169, 0x16a, 0x16a, 0x16b, 0x16c, 0x16c,

    0x16d, 0x16e, 0x16e, 0x16f, 0x170, 0x170, 0x171, 0x172,
    0x172, 0x173, 0x174, 0x174, 0x175, 0x176, 0x176, 0x177,
    0x178, 0x178, 0x179, 0x17a, 0x17a, 0x17b, 0x17c, 0x17c,
    0x17d, 0x17e, 0x17e, 0x17f, 0x180, 0x181, 0x181, 0x182,

    0x183, 0x183, 0x184, 0x185, 0x185, 0x186, 0x187, 0x188,
    0x188, 0x189, 0x18a, 0x18a, 0x18b, 0x18c, 0x18d, 0x18d,
    0x18e, 0x18f, 0x18f, 0x190, 0x191, 0x192, 0x192, 0x193,
    0x194, 0x194, 0x195, 0x196, 0x197, 0x197, 0x198, 0x199,

    0x19a, 0x19a, 0x19b, 0x19c, 0x19d, 0x19d, 0x19e, 0x19f,
    0x1a0, 0x1a0, 0x1a1, 0x1a2, 0x1a3, 0x1a3, 0x1a4, 0x1a5,
    0x1a6, 0x1a6, 0x1a7, 0x1a8, 0x1a9, 0x1a9, 0x1aa, 0x1ab,
    0x1ac, 0x1ad, 0x1ad, 0x1ae, 0x1af, 0x1b0, 0x1b0, 0x1b1,

    0x1b2, 0x1b3, 0x1b4, 0x1b4, 0x1b5, 0x1b6, 0x1b7, 0x1b8,
    0x1b8, 0x1b9, 0x1ba, 0x1bb, 0x1bc, 0x1bc, 0x1bd, 0x1be,
    0x1bf, 0x1c0, 0x1c0, 0x1c1, 0x1c2, 0x1c3, 0x1c4, 0x1c4,
    0x1c5, 0x1c6, 0x1c7, 0x1c8, 0x1c9, 0x1c9, 0x1ca, 0x1cb,

    0x1cc, 0x1cd, 0x1ce, 0x1ce, 0x1cf, 0x1d0, 0x1d1, 0x1d2,
    0x1d3, 0x1d3, 0x1d4, 0x1d5, 0x1d6, 0x1d7, 0x1d8, 0x1d8,
    0x1d9, 0x1da, 0x1db, 0x1dc, 0x1dd, 0x1de, 0x1de, 0x1df,
    0x1e0, 0x1e1, 0x1e2, 0x1e3, 0x1e4, 0x1e5, 0x1e5, 0x1e6,

    0x1e7, 0x1e8, 0x1e9, 0x1ea, 0x1eb, 0x1ec, 0x1ed, 0x1ed,
    0x1ee, 0x1ef, 0x1f0, 0x1f1, 0x1f2, 0x1f3, 0x1f4, 0x1f5,
    0x1f6, 0x1f6, 0x1f7, 0x1f8, 0x1f9, 0x1fa, 0x1fb, 0x1fc,
    0x1fd, 0x1fe, 0x1ff, 0x200, 0x201, 0x201, 0x202, 0x203,

    // First note of looped range used for all octaves:
    0x204, 0x205, 0x206, 0x207, 0x208, 0x209, 0x20a, 0x20b,
    0x20c, 0x20d, 0x20e, 0x20f, 0x210, 0x210, 0x211, 0x212,
    0x213, 0x214, 0x215, 0x216, 0x217, 0x218, 0x219, 0x21a,
    0x21b, 0x21c, 0x21d, 0x21e, 0x21f, 0x220, 0x221, 0x222,

    0x223, 0x224, 0x225, 0x226, 0x227, 0x228, 0x229, 0x22a,
    0x22b, 0x22c, 0x22d, 0x22e, 0x22f, 0x230, 0x231, 0x232,
    0x233, 0x234, 0x235, 0x236, 0x237, 0x238, 0x239, 0x23a,
    0x23b, 0x23c, 0x23d, 0x23e, 0x23f, 0x240, 0x241, 0x242,

    0x244, 0x245, 0x246, 0x247, 0x248, 0x249, 0x24a, 0x24b,
    0x24c, 0x24d, 0x24e, 0x24f, 0x250, 0x251, 0x252, 0x253,
    0x254, 0x256, 0x257, 0x258, 0x259, 0x25a, 0x25b, 0x25c,
    0x25d, 0x25e, 0x25f, 0x260, 0x262, 0x263, 0x264, 0x265,

    0x266, 0x267, 0x268, 0x269, 0x26a, 0x26c, 0x26d, 0x26e,
    0x26f, 0x270, 0x271, 0x272, 0x273, 0x275, 0x276, 0x277,
    0x278, 0x279, 0x27a, 0x27b, 0x27d, 0x27e, 0x27f, 0x280,
    0x281, 0x282, 0x284, 0x285, 0x286, 0x287, 0x288, 0x289,

    0x28b, 0x28c, 0x28d, 0x28e, 0x28f, 0x290, 0x292, 0x293,
    0x294, 0x295, 0x296, 0x298, 0x299, 0x29a, 0x29b, 0x29c,
    0x29e, 0x29f, 0x2a0, 0x2a1, 0x2a2, 0x2a4, 0x2a5, 0x2a6,
    0x2a7, 0x2a9, 0x2aa, 0x2ab, 0x2ac, 0x2ae, 0x2af, 0x2b0,

    0x2b1, 0x2b2, 0x2b4, 0x2b5, 0x2b6, 0x2b7, 0x2b9, 0x2ba,
    0x2bb, 0x2bd, 0x2be, 0x2bf, 0x2c0, 0x2c2, 0x2c3, 0x2c4,
    0x2c5, 0x2c7, 0x2c8, 0x2c9, 0x2cb, 0x2cc, 0x2cd, 0x2ce,
    0x2d0, 0x2d1, 0x2d2, 0x2d4, 0x2d5, 0x2d6, 0x2d8, 0x2d9,

    0x2da, 0x2dc, 0x2dd, 0x2de, 0x2e0, 0x2e1, 0x2e2, 0x2e4,
    0x2e5, 0x2e6, 0x2e8, 0x2e9, 0x2ea, 0x2ec, 0x2ed, 0x2ee,
    0x2f0, 0x2f1, 0x2f2, 0x2f4, 0x2f5, 0x2f6, 0x2f8, 0x2f9,
    0x2fb, 0x2fc, 0x2fd, 0x2ff, 0x300, 0x302, 0x303, 0x304,

    0x306, 0x307, 0x309, 0x30a, 0x30b, 0x30d, 0x30e, 0x310,
    0x311, 0x312, 0x314, 0x315, 0x317, 0x318, 0x31a, 0x31b,
    0x31c, 0x31e, 0x31f, 0x321, 0x322, 0x324, 0x325, 0x327,
    0x328, 0x329, 0x32b, 0x32c, 0x32e, 0x32f, 0x331, 0x332,

    0x334, 0x335, 0x337, 0x338, 0x33a, 0x33b, 0x33d, 0x33e,
    0x340, 0x341, 0x343, 0x344, 0x346, 0x347, 0x349, 0x34a,
    0x34c, 0x34d, 0x34f, 0x350, 0x352, 0x353, 0x355, 0x357,
    0x358, 0x35a, 0x35b, 0x35d, 0x35e, 0x360, 0x361, 0x363,

    0x365, 0x366, 0x368, 0x369, 0x36b, 0x36c, 0x36e, 0x370,
    0x371, 0x373, 0x374, 0x376, 0x378, 0x379, 0x37b, 0x37c,
    0x37e, 0x380, 0x381, 0x383, 0x384, 0x386, 0x388, 0x389,
    0x38b, 0x38d, 0x38e, 0x390, 0x392, 0x393, 0x395, 0x397,

    0x398, 0x39a, 0x39c, 0x39d, 0x39f, 0x3a1, 0x3a2, 0x3a4,
    0x3a6, 0x3a7, 0x3a9, 0x3ab, 0x3ac, 0x3ae, 0x3b0, 0x3b1,
    0x3b3, 0x3b5, 0x3b7, 0x3b8, 0x3ba, 0x3bc, 0x3bd, 0x3bf,
    0x3c1, 0x3c3, 0x3c4, 0x3c6, 0x3c8, 0x3ca, 0x3cb, 0x3cd,

    // The last note has an incomplete range
    0x3cf, 0x3d1, 0x3d2, 0x3d4, 0x3d6, 0x3d8, 0x3da, 0x3db,
    0x3dd, 0x3df, 0x3e1, 0x3e3, 0x3e4, 0x3e6, 0x3e8, 0x3ea,
    0x3ec, 0x3ed, 0x3ef, 0x3f1, 0x3f3, 0x3f5, 0x3f6, 0x3f8,
    0x3fa, 0x3fc, 0x3fe, 0x36c
};

// Channel state
typedef struct {
    int voice;           // OPL voice assigned (-1 if none)
    int instrument;      // Current instrument (patch)
    int volume;          // Volume (0-127)
    int pan;             // Pan (0-127, 64=center)
    int bend;            // Pitch bend value
    int active;          // Is note active?
    int note;            // Current note
    int key;             // MIDI key number
    int velocity;        // Last velocity for note-on (0-127)
} channel_state_t;

// Voice state
typedef struct {
    int index;           // Voice index (0-17)
    int op1, op2;        // Operator indices
    int array;           // OPL array (0 or 0x100)
    genmidi_instr_t* current_instr;  // Current instrument
    unsigned int current_instr_voice; // Voice number in instrument
    channel_state_t* channel;        // Channel using this voice
    unsigned int key;    // MIDI key
    unsigned int note;   // Note being played
    unsigned int freq;   // Current frequency
    unsigned int car_volume;  // Carrier volume register value
    unsigned int mod_volume;  // Modulator volume register value
    unsigned int note_volume; // Note volume
    unsigned int reg_pan;     // Pan register value
    int in_use;          // Is this voice in use?
} voice_state_t;

// MUS player state
struct mus_player_s {
    opl3_chip opl;                    // OPL3 chip state
    const uint8_t* data;              // MUS data pointer
    size_t data_size;                 // MUS data size
    const uint8_t* score;             // Score pointer
    size_t score_size;                // Score size
    const uint8_t* position;          // Current position in score
    int playing;                      // Is playing?
    int looping;                      // Loop enabled?
    uint64_t current_sample;          // Current sample index
    uint64_t next_event_sample;       // Sample index for next event
    uint64_t timing_remainder;        // Remainder for tick->sample conversion
    int sample_rate;                  // Sample rate
    channel_state_t channels[16];     // MIDI channel states
    voice_state_t voices[18];         // OPL voice states
    genmidi_instr_t* instruments;     // Instrument definitions (main)
    genmidi_instr_t* percussion;      // Percussion instruments
    int instruments_loaded;           // Are instruments loaded?
};

// Forward declarations
static void write_opl_reg(mus_player_t* player, int reg, int value);
static void load_operator(mus_player_t* player, int operator_idx, genmidi_op_t* data, int max_level, unsigned int* volume);
static void set_voice_instrument(mus_player_t* player, voice_state_t* voice, genmidi_instr_t* instr, unsigned int instr_voice);
static void set_voice_volume(mus_player_t* player, voice_state_t* voice, unsigned int volume);
static void set_voice_pan(mus_player_t* player, voice_state_t* voice, unsigned int reg_pan);
static void update_voice_frequency(mus_player_t* player, voice_state_t* voice);
static void voice_key_on(mus_player_t* player, channel_state_t* channel, genmidi_instr_t* instrument, unsigned int note, unsigned int key, unsigned int volume);
static void voice_key_off(mus_player_t* player, voice_state_t* voice);
static voice_state_t* allocate_voice(mus_player_t* player);
static void replace_voice(mus_player_t* player, channel_state_t* for_channel);
static void release_voice(mus_player_t* player, voice_state_t* voice);
static void release_all_voices_for_channel(mus_player_t* player, channel_state_t* channel);
static unsigned int frequency_for_voice(mus_player_t* player, voice_state_t* voice);
static void set_channel_volume(mus_player_t* player, channel_state_t* channel, unsigned int volume);
static void set_channel_pan(mus_player_t* player, channel_state_t* channel, unsigned int pan);

// Write OPL register
static void write_opl_reg(mus_player_t* player, int reg, int value) {
    OPL3_WriteReg(&player->opl, (Bit16u)reg, (Bit8u)value);
}

// Load operator data to OPL registers (from Chocolate Doom)
static void load_operator(mus_player_t* player, int operator_idx, genmidi_op_t* data, int max_level, unsigned int* volume) {
    int level;
    
    // The scale and level fields must be combined for the level register
    level = data->scale;
    
    if (max_level) {
        level |= 0x3f;
    } else {
        level |= data->level;
    }
    
    *volume = level;
    
    write_opl_reg(player, OPL_REGS_LEVEL + operator_idx, level);
    write_opl_reg(player, OPL_REGS_TREMOLO + operator_idx, data->tremolo);
    write_opl_reg(player, OPL_REGS_ATTACK + operator_idx, data->attack);
    write_opl_reg(player, OPL_REGS_SUSTAIN + operator_idx, data->sustain);
    write_opl_reg(player, OPL_REGS_WAVEFORM + operator_idx, data->waveform);
}

// Set the instrument for a voice (from Chocolate Doom)
static void set_voice_instrument(mus_player_t* player, voice_state_t* voice, genmidi_instr_t* instr, unsigned int instr_voice) {
    genmidi_voice_t* data;
    unsigned int modulating;
    
    // Instrument already set?
    if (voice->current_instr == instr && voice->current_instr_voice == instr_voice) {
        return;
    }
    
    voice->current_instr = instr;
    voice->current_instr_voice = instr_voice;
    
    data = &instr->voices[instr_voice];
    
    // Are we using modulated feedback mode?
    modulating = (data->feedback & 0x01) == 0;
    
    // Doom loads the second operator first, then the first
    // The carrier is set to minimum volume until the voice volume is set
    load_operator(player, voice->op2 | voice->array, &data->carrier, 1, &voice->car_volume);
    load_operator(player, voice->op1 | voice->array, &data->modulator, !modulating, &voice->mod_volume);
    
    // Set feedback register
    write_opl_reg(player, (OPL_REGS_FEEDBACK + voice->index) | voice->array, data->feedback | voice->reg_pan);
}

// Set voice volume (from Chocolate Doom)
static void set_voice_volume(mus_player_t* player, voice_state_t* voice, unsigned int volume) {
    genmidi_voice_t* opl_voice;
    unsigned int midi_volume;
    unsigned int full_volume;
    unsigned int car_volume;
    unsigned int mod_volume;
    
    voice->note_volume = volume;
    
    opl_voice = &voice->current_instr->voices[voice->current_instr_voice];
    
    // Multiply note volume and channel volume
    midi_volume = 2 * (volume_mapping_table[voice->channel->volume] + 1);
    full_volume = (volume_mapping_table[voice->note_volume] * midi_volume) >> 9;
    
    // Clamp to valid range
    if (full_volume > 0x3f) {
        full_volume = 0x3f;
    }
    
    // The volume value to use in the register
    car_volume = 0x3f - full_volume;
    
    // Update the volume register(s) if necessary
    if (car_volume != (voice->car_volume & 0x3f)) {
        voice->car_volume = car_volume | (voice->car_volume & 0xc0);
        
        write_opl_reg(player, (OPL_REGS_LEVEL + voice->op2) | voice->array, voice->car_volume);
        
        // If we are using non-modulated feedback mode, set volume for both voices
        if ((opl_voice->feedback & 0x01) != 0 && opl_voice->modulator.level != 0x3f) {
            mod_volume = opl_voice->modulator.level;
            if (mod_volume < car_volume) {
                mod_volume = car_volume;
            }
            mod_volume |= voice->mod_volume & 0xc0;
            
            if (mod_volume != voice->mod_volume) {
                voice->mod_volume = mod_volume;
                write_opl_reg(player, (OPL_REGS_LEVEL + voice->op1) | voice->array,
                              mod_volume | (opl_voice->modulator.scale & 0xc0));
            }
        }
    }
}

// Set voice pan (OPL3 only; uses reg_pan bits in feedback register)
static void set_voice_pan(mus_player_t* player, voice_state_t* voice, unsigned int reg_pan) {
    genmidi_voice_t* data;

    if (voice->reg_pan == reg_pan || !voice->current_instr) {
        return;
    }

    voice->reg_pan = reg_pan;
    data = &voice->current_instr->voices[voice->current_instr_voice];

    write_opl_reg(player, (OPL_REGS_FEEDBACK + voice->index) | voice->array,
                  data->feedback | voice->reg_pan);
}

static void set_channel_volume(mus_player_t* player, channel_state_t* channel, unsigned int volume) {
    int i;

    if (volume > 127) {
        volume = 127;
    }

    channel->volume = (int)volume;

    // Update all voices that this channel is using.
    for (i = 0; i < 18; i++) {
        if (player->voices[i].in_use && player->voices[i].channel == channel) {
            set_voice_volume(player, &player->voices[i], player->voices[i].note_volume);
        }
    }
}

static void set_channel_pan(mus_player_t* player, channel_state_t* channel, unsigned int pan) {
    unsigned int reg_pan;
    int i;

    // DMX pan mapping: left <= 48, center 49-95, right >= 96
    if (pan >= 96) {
        reg_pan = 0x10;
    } else if (pan <= 48) {
        reg_pan = 0x20;
    } else {
        reg_pan = 0x30;
    }

    if ((unsigned int)channel->pan == reg_pan) {
        return;
    }

    channel->pan = (int)reg_pan;

    for (i = 0; i < 18; i++) {
        if (player->voices[i].in_use && player->voices[i].channel == channel) {
            set_voice_pan(player, &player->voices[i], reg_pan);
        }
    }
}
// Calculate frequency for a voice (from Chocolate Doom)
static unsigned int frequency_for_voice(mus_player_t* player, voice_state_t* voice) {
    genmidi_voice_t* gm_voice;
    signed int freq_index;
    unsigned int octave;
    unsigned int sub_index;
    signed int note;
    
    (void)player;  // Unused
    
    note = voice->note;
    
    // Apply note offset - don't apply if fixed note instrument
    gm_voice = &voice->current_instr->voices[voice->current_instr_voice];
    
    if ((voice->current_instr->flags & GENMIDI_FLAG_FIXED) == 0) {
        note += (signed short)gm_voice->base_note_offset;
    }
    
    // Avoid possible overflow
    while (note < 0) {
        note += 12;
    }
    while (note > 95) {
        note -= 12;
    }
    
    freq_index = 64 + 32 * note + voice->channel->bend;
    
    // If this is the second voice of a double voice instrument, adjust by fine tuning
    if (voice->current_instr_voice != 0) {
        freq_index += (voice->current_instr->fine_tuning / 2) - 64;
    }
    
    if (freq_index < 0) {
        freq_index = 0;
    }
    
    // The first 7 notes use the start of the table
    if (freq_index < 284) {
        return frequency_curve[freq_index];
    }
    
    sub_index = (freq_index - 284) % (12 * 32);
    octave = (freq_index - 284) / (12 * 32);
    
    // Cap at octave 7
    if (octave >= 7) {
        octave = 7;
    }
    
    // Calculate the resulting register value
    return frequency_curve[sub_index + 284] | (octave << 10);
}

// Update voice frequency (from Chocolate Doom)
static void update_voice_frequency(mus_player_t* player, voice_state_t* voice) {
    unsigned int freq;
    
    freq = frequency_for_voice(player, voice);
    
    if (voice->freq != freq) {
        write_opl_reg(player, (OPL_REGS_FREQ_1 + voice->index) | voice->array, freq & 0xff);
        write_opl_reg(player, (OPL_REGS_FREQ_2 + voice->index) | voice->array, (freq >> 8) | 0x20);
        voice->freq = freq;
    }
}

// Turn on a voice (from Chocolate Doom)
// Note: Chocolate Doom does NOT release the previous voice when a new note is played
// The old voice continues until it receives a note off or is stolen
static void voice_key_on(mus_player_t* player, channel_state_t* channel, genmidi_instr_t* instrument, unsigned int note, unsigned int key, unsigned int volume) {
    voice_state_t* voice;
    voice_state_t* voice2 = NULL;
    int double_voice;
    
    // Check if this is a double-voice instrument
    double_voice = (instrument->flags & GENMIDI_FLAG_2VOICE) != 0;
    
    // Allocate first voice - if none available, steal one
    voice = allocate_voice(player);
    if (!voice) {
        replace_voice(player, channel);
        voice = allocate_voice(player);
        if (!voice) {
            return;  // Still can't get a voice
        }
    }
    
    // For double-voice instruments, allocate a second voice
    if (double_voice) {
        voice2 = allocate_voice(player);
        if (!voice2) {
            replace_voice(player, channel);
            voice2 = allocate_voice(player);
            if (!voice2) {
                // Can't get second voice, just use single
                double_voice = 0;
            }
        }
    }
    
    voice->channel = channel;
    voice->key = key;
    
    // Work out the note to use - normally same as key, unless fixed pitch
    if ((instrument->flags & GENMIDI_FLAG_FIXED) != 0) {
        voice->note = instrument->fixed_note;
    } else {
        voice->note = note;
    }
    
    voice->reg_pan = channel->pan;
    
    // Set up the instrument (use first voice)
    set_voice_instrument(player, voice, instrument, 0);
    
    // Set volume
    set_voice_volume(player, voice, volume);
    
    // Write frequency to turn the note on
    voice->freq = 0;
    update_voice_frequency(player, voice);
    
    // Set up second voice for double-voice instruments
    if (double_voice && voice2) {
        voice2->channel = channel;
        voice2->key = key;
        
        // Same note calculation
        if ((instrument->flags & GENMIDI_FLAG_FIXED) != 0) {
            voice2->note = instrument->fixed_note;
        } else {
            voice2->note = note;
        }
        
        voice2->reg_pan = channel->pan;
        
        // Set up the instrument (use second voice)
        set_voice_instrument(player, voice2, instrument, 1);
        
        // Set volume
        set_voice_volume(player, voice2, volume);
        
        // Write frequency to turn the note on
        voice2->freq = 0;
        update_voice_frequency(player, voice2);
    }
}

// Turn off a voice (from Chocolate Doom)
static void voice_key_off(mus_player_t* player, voice_state_t* voice) {
    // Clear key-on bit but preserve the frequency
    write_opl_reg(player, (OPL_REGS_FREQ_2 + voice->index) | voice->array, voice->freq >> 8);
}

// Replace an existing voice (voice stealing) - from Chocolate Doom
static void replace_voice(mus_player_t* player, channel_state_t* for_channel) {
    int i;
    int result = 0;
    int result_channel_idx = 0;
    int voice_channel_idx;
    
    // Find the channel index of the voice we're considering
    for (i = 0; i < 18; i++) {
        if (player->voices[i].in_use) {
            // Get channel index (0-15)
            voice_channel_idx = player->voices[i].channel ? 
                (int)(player->voices[i].channel - player->channels) : 0;
            
            // Second voice of double-voice instrument - best candidate
            if (player->voices[i].current_instr_voice != 0) {
                result = i;
                break;
            }
            
            // Get the channel index of the current result
            result_channel_idx = player->voices[result].channel ?
                (int)(player->voices[result].channel - player->channels) : 0;
            
            // Prefer voices on higher-numbered channels (lower priority)
            // In Chocolate Doom: voice_alloced_list[i]->channel >= voice_alloced_list[result]->channel
            if (voice_channel_idx >= result_channel_idx) {
                result = i;
            }
        }
    }
    
    // Release the selected voice
    if (player->voices[result].in_use) {
        voice_key_off(player, &player->voices[result]);
        if (player->voices[result].channel) {
            player->voices[result].channel->active = 0;
            player->voices[result].channel->voice = -1;
        }
        player->voices[result].in_use = 0;
        player->voices[result].channel = NULL;
    }
}

// Allocate a free voice
static voice_state_t* allocate_voice(mus_player_t* player) {
    int i;
    
    for (i = 0; i < 18; i++) {
        if (!player->voices[i].in_use) {
            player->voices[i].in_use = 1;
            return &player->voices[i];
        }
    }
    
    return NULL;  // No free voices
}

// Release a voice
static void release_voice(mus_player_t* player, voice_state_t* voice) {
    if (!voice || !voice->in_use) return;
    
    voice_key_off(player, voice);
    
    if (voice->channel) {
        voice->channel->active = 0;
        voice->channel->voice = -1;
    }
    
    voice->in_use = 0;
    voice->channel = NULL;
    voice->current_instr = NULL;
}

// Release all voices for a channel (for double-voice instruments)
static void release_all_voices_for_channel(mus_player_t* player, channel_state_t* channel) {
    int i;
    
    for (i = 0; i < 18; i++) {
        if (player->voices[i].in_use && player->voices[i].channel == channel) {
            voice_key_off(player, &player->voices[i]);
            player->voices[i].in_use = 0;
            player->voices[i].channel = NULL;
            player->voices[i].current_instr = NULL;
        }
    }
    
    channel->active = 0;
    channel->voice = -1;
}

// Initialize OPL registers (from Chocolate Doom)
static void init_opl_registers(mus_player_t* player) {
    int r;
    
    // Initialize level registers for first array
    for (r = OPL_REGS_LEVEL; r <= OPL_REGS_LEVEL + 21; ++r) {
        write_opl_reg(player, r, 0x3f);
    }
    
    // Initialize other registers
    for (r = OPL_REGS_ATTACK; r <= OPL_REGS_WAVEFORM + 21; ++r) {
        write_opl_reg(player, r, 0x00);
    }
    
    // More registers
    for (r = 1; r < OPL_REGS_LEVEL; ++r) {
        write_opl_reg(player, r, 0x00);
    }
    
    // Reset both timers and enable interrupts
    write_opl_reg(player, 0x04, 0x60);
    write_opl_reg(player, 0x04, 0x80);
    
    // "Allow FM chips to control the waveform of each operator"
    write_opl_reg(player, 0x01, 0x20);
    
    // Enable OPL3 mode
    write_opl_reg(player, 0x105, 0x01);
    
    // Initialize level registers for second array (OPL3)
    for (r = OPL_REGS_LEVEL; r <= OPL_REGS_LEVEL + 21; ++r) {
        write_opl_reg(player, r | 0x100, 0x3f);
    }
    
    // Initialize other registers for second array
    for (r = OPL_REGS_ATTACK; r <= OPL_REGS_WAVEFORM + 21; ++r) {
        write_opl_reg(player, r | 0x100, 0x00);
    }
    
    // More registers for second array
    for (r = 1; r < OPL_REGS_LEVEL; ++r) {
        write_opl_reg(player, r | 0x100, 0x00);
    }
}

// Create MUS player
mus_player_t* mus_player_create(int sample_rate) {
    mus_player_t* player;
    int i;
    
    player = (mus_player_t*)calloc(1, sizeof(mus_player_t));
    if (!player) return NULL;
    
    player->sample_rate = sample_rate;
    
    // Initialize OPL3
    OPL3_Reset(&player->opl, sample_rate);
    
    // Initialize OPL registers
    init_opl_registers(player);
    
    // Initialize channels
    for (i = 0; i < 16; i++) {
        player->channels[i].voice = -1;
        player->channels[i].instrument = 0;
        player->channels[i].volume = 100;
        player->channels[i].pan = 0x30;  // Center pan (both left and right)
        player->channels[i].bend = 0;
        player->channels[i].active = 0;
        player->channels[i].note = 0;
        player->channels[i].velocity = 127;
    }
    
    // Initialize voices
    for (i = 0; i < 18; i++) {
        player->voices[i].index = i % 9;
        player->voices[i].op1 = voice_operators[0][i % 9];
        player->voices[i].op2 = voice_operators[1][i % 9];
        player->voices[i].array = (i / 9) << 8;
        player->voices[i].in_use = 0;
        player->voices[i].current_instr = NULL;
        player->voices[i].channel = NULL;
        player->voices[i].reg_pan = 0x30;  // Center pan
        player->voices[i].freq = 0;
    }
    
    // Allocate instrument arrays
    player->instruments = (genmidi_instr_t*)calloc(256, sizeof(genmidi_instr_t));
    player->percussion = (genmidi_instr_t*)calloc(256, sizeof(genmidi_instr_t));
    
    if (!player->instruments || !player->percussion) {
        free(player->instruments);
        free(player->percussion);
        free(player);
        return NULL;
    }
    
    return player;
}

// Destroy MUS player
void mus_player_destroy(mus_player_t* player) {
    if (!player) return;
    free(player->instruments);
    free(player->percussion);
    free(player);
}

// Load MUS data
int mus_player_load(mus_player_t* player, const uint8_t* data, size_t size) {
    const mus_header_t* header;
    
    if (!player || !data || size < sizeof(mus_header_t)) {
        return -1;
    }
    
    header = (const mus_header_t*)data;
    
    // Check MUS signature
    if (header->id[0] != 'M' || header->id[1] != 'U' || 
        header->id[2] != 'S' || header->id[3] != 0x1a) {
        return -1;
    }
    
    player->data = data;
    player->data_size = size;
    player->score = data + header->score_start;
    player->score_size = header->score_len;
    player->position = player->score;
    player->playing = 0;
    player->current_sample = 0;
    player->next_event_sample = 0;
    player->timing_remainder = 0;
    
    return 0;
}

// Load GENMIDI instruments
int mus_player_load_instruments(mus_player_t* player, const uint8_t* data, size_t size) {
    const uint8_t* ptr;
    int i;
    
    if (!player || !data || size < 8) {
        return -1;
    }
    
    // Check GENMIDI signature
    if (memcmp(data, "#OPL_II#", 8) != 0) {
        return -1;
    }
    
    ptr = data + 8;  // Skip header
    
    // Load main instruments (128 melodic instruments)
    for (i = 0; i < 128; i++) {
        memcpy(&player->instruments[i], ptr, sizeof(genmidi_instr_t));
        ptr += sizeof(genmidi_instr_t);
    }
    
    // Load percussion instruments (47 percussion instruments)
    // Percussion is indexed by note number (35-81 are standard)
    for (i = 0; i < 47; i++) {
        memcpy(&player->percussion[i], ptr, sizeof(genmidi_instr_t));
        ptr += sizeof(genmidi_instr_t);
    }
    
    player->instruments_loaded = 1;
    return 0;
}

// Start playback
void mus_player_start(mus_player_t* player, int looping) {
    if (!player || !player->data) return;
    
    player->looping = looping;
    player->playing = 1;
    player->position = player->score;
    player->current_sample = 0;
    player->next_event_sample = 0;
    player->timing_remainder = 0;
}

// Stop playback
void mus_player_stop(mus_player_t* player) {
    if (!player) return;
    player->playing = 0;
}

// Check if playing
int mus_player_is_playing(mus_player_t* player) {
    if (!player) return 0;
    return player->playing;
}

// Read variable-length quantity
static uint32_t read_varlen(const uint8_t** ptr) {
    uint32_t value = 0;
    uint8_t byte;
    
    do {
        byte = *(*ptr)++;
        value = (value << 7) | (byte & 0x7f);
    } while (byte & 0x80);
    
    return value;
}

// Process one MUS event
static void process_event(mus_player_t* player);
static void advance_event_time(mus_player_t* player, uint32_t delay_ticks);

static void process_event(mus_player_t* player) {
    uint8_t event, channel, type;
    const uint8_t* ptr;
    uint32_t delay;
    
    if (!player->position || player->position >= player->score + player->score_size) {
        if (player->looping) {
            player->position = player->score;
            player->current_sample = 0;
            player->next_event_sample = 0;
            player->timing_remainder = 0;
        } else {
            player->playing = 0;
        }
        return;
    }
    
    ptr = player->position;
    
    // Read event
    event = *ptr++;
    channel = event & 0x0f;
    type = event & 0x70;
    
    // MUS channel 15 maps to MIDI channel 9 (percussion)
    if (channel == 15) channel = 9;
    else if (channel == 9) channel = 15;  // Avoid conflict
    
    switch (type) {
        case MUS_EVENT_RELEASE_NOTE: {
            uint8_t note = *ptr++;
            // Chocolate Doom searches for voices by channel AND key
            // This handles both melodic and percussion correctly
            // For double-voice instruments, both voices will be released
            int i;
            for (i = 0; i < 18; i++) {
                if (player->voices[i].in_use && 
                    player->voices[i].channel == &player->channels[channel] &&
                    player->voices[i].key == note) {
                    voice_key_off(player, &player->voices[i]);
                    player->voices[i].in_use = 0;
                    player->voices[i].channel = NULL;
                    player->voices[i].current_instr = NULL;
                    // Don't break - continue searching for double-voice instruments
                }
            }
            break;
        }
        case MUS_EVENT_PLAY_NOTE: {
            uint8_t note_data = *ptr++;
            uint8_t note = note_data & 0x7f;
            uint8_t velocity = (uint8_t)player->channels[channel].velocity;
            genmidi_instr_t* instr;
            
            if (note_data & 0x80) {
                velocity = *ptr++;
                velocity &= 0x7f;
                player->channels[channel].velocity = velocity;
            }
            
            // A volume of zero means key off (from Chocolate Doom)
            if (velocity <= 0) {
                // Treat as note off - find voices by channel and key
                int i;
                for (i = 0; i < 18; i++) {
                    if (player->voices[i].in_use && 
                        player->voices[i].channel == &player->channels[channel] &&
                        player->voices[i].key == note) {
                        voice_key_off(player, &player->voices[i]);
                        player->voices[i].in_use = 0;
                        player->voices[i].channel = NULL;
                        player->voices[i].current_instr = NULL;
                    }
                }
                break;
            }
            
            // Channel 9 is percussion - use note as instrument index
            if (channel == 9) {
                // MIDI percussion notes start at 35 (kick drum)
                int perc_index = note - 35;
                if (perc_index >= 0 && perc_index < 47) {
                    instr = &player->percussion[perc_index];
                } else {
                    instr = &player->instruments[0];  // Fallback
                }
                // Chocolate Doom uses note=60 for percussion, key=actual key
                // The instrument's fixed_note will be used if GENMIDI_FLAG_FIXED is set
                if (player->instruments_loaded) {
                    voice_key_on(player, &player->channels[channel], instr, 60, note, velocity);
                }
            } else {
                // Melodic instrument
                instr = &player->instruments[player->channels[channel].instrument];
                if (player->instruments_loaded) {
                    voice_key_on(player, &player->channels[channel], instr, note, note, velocity);
                }
            }
            break;
        }
        case MUS_EVENT_PITCH_BEND: {
            uint8_t bend = *ptr++;
            // MUS pitch bend: 0-255, where 128 is center
            // Chocolate Doom uses: bend - 128 for the offset
            // But OPL expects bend in range -64 to +64 where 0 is center
            // So we need to scale: (bend - 128) / 2 = bend/2 - 64
            player->channels[channel].bend = ((int)(bend) - 128) / 2;
            
            // Update all active voices on this channel
            {
                int i;
                for (i = 0; i < 18; i++) {
                    if (player->voices[i].in_use && 
                        player->voices[i].channel == &player->channels[channel]) {
                        player->voices[i].freq = 0;  // Force frequency update
                        update_voice_frequency(player, &player->voices[i]);
                    }
                }
            }
            break;
        }
        case MUS_EVENT_SYSTEM_EVENT: {
            uint8_t sys_event = *ptr++;
            // Map MUS system events 10-14 to MIDI controllers.
            // 10: All sounds off (0x78), 11: All notes off (0x7B),
            // 12: Mono (0x7E), 13: Poly (0x7F), 14: Reset controllers (0x79)
            switch (sys_event) {
                case 10: // All sounds off
                case 11: // All notes off
                    release_all_voices_for_channel(player, &player->channels[channel]);
                    break;
                case 14: // Reset all controllers
                    set_channel_volume(player, &player->channels[channel], 100);
                    set_channel_pan(player, &player->channels[channel], 64);
                    player->channels[channel].bend = 0;
                    break;
                default:
                    break;
            }
            break;
        }
        case MUS_EVENT_CONTROLLER: {
            uint8_t ctrl = *ptr++;
            uint8_t value = *ptr++;
            if (ctrl < 15) {
                int midi_ctrl = mus_to_midi_ctrl[ctrl];
                if (midi_ctrl == 0 && ctrl == 0) {
                    // Program change
                    player->channels[channel].instrument = value;
                } else if (midi_ctrl == 7) {
                    // Volume
                    set_channel_volume(player, &player->channels[channel], value);
                } else if (midi_ctrl == 10) {
                    // Pan
                    set_channel_pan(player, &player->channels[channel], value);
                }
            }
            break;
        }
        case MUS_EVENT_END_OF_SCORE:
            if (player->looping) {
                player->position = player->score;
            player->current_sample = 0;
            player->next_event_sample = 0;
            player->timing_remainder = 0;
            } else {
                player->playing = 0;
            }
            return;
    }
    
    // Check for delay
    if (event & 0x80) {
        delay = read_varlen(&ptr);
        advance_event_time(player, delay);
    }
    
    player->position = ptr;
}

// Advance next_event_sample by delay ticks (140 Hz), keeping exact fractional remainder.
static void advance_event_time(mus_player_t* player, uint32_t delay_ticks) {
    uint64_t accum = player->timing_remainder
                   + (uint64_t)delay_ticks * (uint64_t)player->sample_rate;
    player->next_event_sample += accum / 140;
    player->timing_remainder = accum % 140;
}

// Generate samples
size_t mus_player_generate(mus_player_t* player, int16_t* buffer, size_t num_samples) {
    size_t samples_generated = 0;
    
    if (!player || !buffer) return 0;
    
    while (samples_generated < num_samples) {
        // Process all events that are due at or before this sample
        while (player->playing && player->current_sample >= player->next_event_sample) {
            process_event(player);
        }
        
        // Generate one sample at the current time
        OPL3_GenerateResampled(&player->opl, buffer);
        buffer += 2;  // Stereo
        samples_generated++;

        // Advance time after generating the sample
        if (player->playing) {
            player->current_sample++;
        }
    }
    
    return samples_generated;
}

// Get position in milliseconds
uint32_t mus_player_get_position_ms(mus_player_t* player) {
    if (!player) return 0;
    return (uint32_t)((player->current_sample * 1000ULL) / player->sample_rate);
}
