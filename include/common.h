// Shared basic types. With HOST_TEST defined (tests/unit), the GBA headers
// are replaced by a shim so the game modules compile and run on a PC.
#ifndef COMMON_H
#define COMMON_H

#ifdef HOST_TEST
#include "host_shim.h"      // tests/unit: fake registers and SRAM
#else
#include <tonc.h>
#endif

#define WORD_LEN    5
#define MAX_GUESSES 6

#endif
