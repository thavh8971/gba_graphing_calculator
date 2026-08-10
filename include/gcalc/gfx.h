#ifndef GCALC_GFX_H
#define GCALC_GFX_H

#include "gcalc/types.h"

#define GBA_SCREEN_WIDTH 240
#define GBA_SCREEN_HEIGHT 160
#define RGB15(r, g, b) ((u16)((r) | ((g) << 5) | ((b) << 10)))

/* Compact printable-ASCII font intended for graph labels/status text. */
#define GFX_ASCII5X7_WIDTH 5
#define GFX_ASCII5X7_HEIGHT 7
#define GFX_ASCII5X7_ADVANCE 6
#define GFX_ASCII5X7_LINE_ADVANCE 8

typedef struct GfxSurface {
    volatile u16 *pixels;
    s16 width;
    s16 height;
    s16 stride;
} GfxSurface;

void gfxClear(GfxSurface *surface, u16 color);
void gfxPixel(GfxSurface *surface, s16 x, s16 y, u16 color);
void gfxFillRect(GfxSurface *surface, s16 x, s16 y, s16 width, s16 height,
                 u16 color);
void gfxRect(GfxSurface *surface, s16 x, s16 y, s16 width, s16 height,
             u16 color);
void gfxLine(GfxSurface *surface, s16 x0, s16 y0, s16 x1, s16 y1,
             u16 color);
void gfxChar(GfxSurface *surface, s16 x, s16 y, char character, u16 color);
void gfxText(GfxSurface *surface, s16 x, s16 y, const char *text, u16 color);
u8 gfxGlyph5x7Row(char character, u8 row);
void gfxChar5x7(GfxSurface *surface, s16 x, s16 y, char character,
                u16 color);
void gfxText5x7(GfxSurface *surface, s16 x, s16 y, const char *text,
                u16 color);

#endif
