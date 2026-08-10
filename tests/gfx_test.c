#include <stdio.h>

#include "gcalc/gfx.h"

#define TEST_WIDTH 16
#define TEST_HEIGHT 12
#define TEST_STRIDE 20
#define GUARD_WORDS 16
#define SENTINEL 0x5aa5

#if GFX_ASCII5X7_WIDTH != 5 || GFX_ASCII5X7_HEIGHT != 7
#error "The compact graph font must have an exact 5x7 bitmap cell"
#endif

static int testFillRectClippingAndStride(void)
{
    u16 storage[GUARD_WORDS + TEST_STRIDE * TEST_HEIGHT + GUARD_WORDS];
    GfxSurface surface;
    u16 index;
    u16 x;
    u16 y;

    for (index = 0; index < GCALC_ARRAY_COUNT(storage); index++)
        storage[index] = SENTINEL;
    surface.pixels = storage + GUARD_WORDS;
    surface.width = TEST_WIDTH;
    surface.height = TEST_HEIGHT;
    surface.stride = TEST_STRIDE;

    /* [-3,5) x [-2,4) clips to [0,5) x [0,4). */
    gfxFillRect(&surface, -3, -2, 8, 6, 0x1234);
    for (y = 0; y < TEST_HEIGHT; y++) {
        for (x = 0; x < TEST_STRIDE; x++) {
            u16 expected = x < 5 && y < 4 ? 0x1234 : SENTINEL;
            if (surface.pixels[y * TEST_STRIDE + x] != expected) {
                fprintf(stderr, "FAIL clipped fill pixel %u,%u\n", x, y);
                return 0;
            }
        }
    }
    for (index = 0; index < GUARD_WORDS; index++) {
        if (storage[index] != SENTINEL ||
            storage[GUARD_WORDS + TEST_STRIDE * TEST_HEIGHT + index] !=
                SENTINEL) {
            fprintf(stderr, "FAIL clipped fill guard word %u\n", index);
            return 0;
        }
    }

    /* A bottom/right overflow must touch only the final 2x2 visible pixels;
       stride padding remains sentinel data. */
    gfxFillRect(&surface, TEST_WIDTH - 2, TEST_HEIGHT - 2, 10, 10, 0x4321);
    for (y = 0; y < TEST_HEIGHT; y++) {
        for (x = 0; x < TEST_STRIDE; x++) {
            u16 expected = SENTINEL;
            if (x < 5 && y < 4)
                expected = 0x1234;
            if (x >= TEST_WIDTH - 2 && x < TEST_WIDTH &&
                y >= TEST_HEIGHT - 2)
                expected = 0x4321;
            if (surface.pixels[y * TEST_STRIDE + x] != expected) {
                fprintf(stderr, "FAIL edge fill pixel %u,%u\n", x, y);
                return 0;
            }
        }
    }
    return 1;
}

int main(void)
{
    u16 storage[GUARD_WORDS + TEST_STRIDE * TEST_HEIGHT + GUARD_WORDS];
    GfxSurface surface;
    u16 index;
    u8 changed = 0;
    u16 code;

    if (!testFillRectClippingAndStride())
        return 1;

    for (index = 0; index < GCALC_ARRAY_COUNT(storage); index++)
        storage[index] = SENTINEL;
    surface.pixels = storage + GUARD_WORDS;
    surface.width = TEST_WIDTH;
    surface.height = TEST_HEIGHT;
    surface.stride = TEST_STRIDE;

    gfxClear(&surface, 0x0123);
    gfxPixel(&surface, -1, -1, 0x7fff);
    gfxPixel(&surface, TEST_WIDTH, TEST_HEIGHT, 0x7fff);
    gfxFillRect(&surface, -8, -8, 12, 12, 0x0456);
    gfxRect(&surface, -3, 2, 24, 8, 0x0789);
    gfxLine(&surface, -100, -50, 100, 50, 0x0111);
    gfxChar(&surface, 13, 8, '~', 0x0222);
    gfxText(&surface, 1, 1, "A!", 0x0333);

    for (code = 0x20; code <= 0x7e; code++) {
        u8 row;
        for (row = 0; row < GFX_ASCII5X7_HEIGHT; row++) {
            if ((gfxGlyph5x7Row((char)code, row) & ~0x1fu) != 0) {
                fprintf(stderr, "FAIL 5x7 glyph width %u row %u\n",
                        code, row);
                return 1;
            }
        }
        if (gfxGlyph5x7Row((char)code, GFX_ASCII5X7_HEIGHT) != 0) {
            fprintf(stderr, "FAIL 5x7 glyph height %u\n", code);
            return 1;
        }
    }
    if (gfxGlyph5x7Row('|', 0) != 0x04 ||
        gfxGlyph5x7Row('|', 6) != 0x04 ||
        gfxGlyph5x7Row('\x01', 3) != gfxGlyph5x7Row('?', 3)) {
        fputs("FAIL 5x7 glyph lookup\n", stderr);
        return 1;
    }

    gfxClear(&surface, 0);
    gfxChar5x7(&surface, 2, 2, '|', 0x0444);
    for (index = 0; index < TEST_HEIGHT; index++) {
        u16 expected = (index >= 2 && index < 2 + GFX_ASCII5X7_HEIGHT) ?
                       0x0444 : 0;
        if (surface.pixels[index * TEST_STRIDE + 4] != expected) {
            fprintf(stderr, "FAIL 5x7 rendered height row %u\n", index);
            return 1;
        }
    }

    gfxClear(&surface, 0);
    gfxText5x7(&surface, 0, 0, "||", 0x0555);
    if (surface.pixels[2] != 0x0555 ||
        surface.pixels[GFX_ASCII5X7_ADVANCE + 2] != 0x0555) {
        fputs("FAIL 5x7 text advance\n", stderr);
        return 1;
    }
    gfxClear(&surface, 0);
    gfxText5x7(&surface, 0, 0, "|\n|", 0x0666);
    if (surface.pixels[2] != 0x0666 ||
        surface.pixels[7 * TEST_STRIDE + 2] != 0 ||
        surface.pixels[GFX_ASCII5X7_LINE_ADVANCE * TEST_STRIDE + 2] !=
            0x0666) {
        fputs("FAIL 5x7 line advance\n", stderr);
        return 1;
    }
    gfxChar5x7(&surface, -3, -3, '#', 0x0777);
    gfxText5x7(&surface, 14, 10, "~~", 0x0777);

    for (index = 0; index < GUARD_WORDS; index++) {
        if (storage[index] != SENTINEL ||
            storage[GUARD_WORDS + TEST_STRIDE * TEST_HEIGHT + index] !=
                SENTINEL) {
            fprintf(stderr, "FAIL gfx guard word %u\n", index);
            return 1;
        }
    }
    for (index = 0; index < TEST_STRIDE * TEST_HEIGHT; index++) {
        u16 column = (u16)(index % TEST_STRIDE);
        if (column >= TEST_WIDTH && storage[GUARD_WORDS + index] != SENTINEL) {
            fprintf(stderr, "FAIL gfx stride padding %u\n", index);
            return 1;
        }
        if (column < TEST_WIDTH &&
            storage[GUARD_WORDS + index] != 0)
            changed = 1;
    }
    if (!changed) {
        fputs("FAIL gfx produced no pixels\n", stderr);
        return 1;
    }
    puts("gfx_test: PASS");
    return 0;
}
