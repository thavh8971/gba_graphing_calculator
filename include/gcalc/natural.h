#ifndef GCALC_NATURAL_H
#define GCALC_NATURAL_H

#include "gcalc/calc.h"

#define NATURAL_MAX_TEXT 127

typedef enum NaturalSlot {
    NATURAL_SLOT_MAIN,
    NATURAL_SLOT_NUMERATOR,
    NATURAL_SLOT_DENOMINATOR,
    NATURAL_SLOT_BASE,
    NATURAL_SLOT_EXPONENT,
    NATURAL_SLOT_INDEX,
    NATURAL_SLOT_RADICAND,
    NATURAL_SLOT_BODY,
    NATURAL_SLOT_VARIABLE,
    NATURAL_SLOT_LOWER,
    NATURAL_SLOT_UPPER,
    NATURAL_SLOT_EVALUATION
} NaturalSlot;

typedef struct NaturalCursor {
    u8 node;
    NaturalSlot slot;
    u8 offset;
} NaturalCursor;

void naturalCursorSetEnd(NaturalCursor *cursor, u8 length);
void naturalCursorRecompute(const char *text, u8 length,
                            NaturalCursor *cursor);
void naturalCursorMoveHorizontal(const char *text, u8 length,
                                 NaturalCursor *cursor, s8 direction);
void naturalCursorMoveVertical(const char *text, u8 length,
                               NaturalCursor *cursor, s8 direction);
u8 naturalCursorInsert(char *text, u8 *length, u8 capacity,
                       const char *insert, NaturalCursor *cursor);
u8 naturalCursorBackspace(char *text, u8 *length,
                          NaturalCursor *cursor);

#endif
