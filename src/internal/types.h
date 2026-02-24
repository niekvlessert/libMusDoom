/**
 * Internal type definitions for libMusDoom
 */

#ifndef MUSDOOM_INTERNAL_TYPES_H
#define MUSDOOM_INTERNAL_TYPES_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

// Type definitions compatible with Chocolate Doom and Nuked OPL3
typedef uintptr_t Bitu;
typedef intptr_t Bits;
typedef uint64_t Bit64u;
typedef int64_t Bit64s;
typedef uint32_t Bit32u;
typedef int32_t Bit32s;
typedef uint16_t Bit16u;
typedef int16_t Bit16s;
typedef uint8_t Bit8u;
typedef int8_t Bit8s;

// Byte type (used by Chocolate Doom code)
typedef uint8_t byte;

// Boolean type
typedef bool boolean;

// Array length macro
#define arrlen(array) (sizeof(array) / sizeof(*array))

// Min/Max macros
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))

// MIDI constants
#define MIDI_CHANNELS_PER_TRACK 16
#define MIDI_PERCUSSION_CHAN 9
#define MUS_PERCUSSION_CHAN 15

// OPL constants
#define OPL_NUM_OPERATORS 21
#define OPL_NUM_VOICES 9
#define OPL_WRITEBUF_SIZE 1024
#define OPL_WRITEBUF_DELAY 2

// Sample rate
#define DEFAULT_SAMPLE_RATE 44100

#endif /* MUSDOOM_INTERNAL_TYPES_H */
