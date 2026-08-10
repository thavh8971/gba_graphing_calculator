#ifndef GCALC_TYPES_H
#define GCALC_TYPES_H

#include <stdint.h>
#include <stddef.h>

typedef uint8_t u8;
typedef int8_t s8;
typedef uint16_t u16;
typedef int16_t s16;
typedef uint32_t u32;
typedef int32_t s32;
typedef uint64_t u64;
typedef int64_t s64;

#define GCALC_ARRAY_COUNT(array) (sizeof(array) / sizeof((array)[0]))

/*
 * devkitARM's GBA linker script maps zero-initialized .sbss objects to
 * external work RAM and clears that section during CRT startup.  Keep the
 * attribute behind a project macro so common sources remain valid host C.
 */
#if defined(GCALC_GBA_ARM_FAST)
#define GCALC_EWRAM_BSS __attribute__((section(".sbss")))
#else
#define GCALC_EWRAM_BSS
#endif

#endif
