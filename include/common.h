// Shared basic types. The game logic (logic.c) is platform independent so it
// can be unit-tested on the host: it only needs the fixed-width typedefs.
#ifndef COMMON_H
#define COMMON_H

#ifdef HOST_TEST
#include <stdint.h>
#include <stdbool.h>
typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef int8_t   s8;
typedef int16_t  s16;
typedef int32_t  s32;
#else
#include <tonc.h>
#endif

#define WORD_LEN    5
#define MAX_GUESSES 6

#endif
