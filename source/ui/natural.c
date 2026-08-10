#include "gcalc/natural.h"

#include <string.h>

#define NATURAL_MAX_STRUCTURES 64
#define NATURAL_MAX_SLOTS 4

typedef enum NaturalStructureKind {
    NATURAL_STRUCTURE_FRACTION,
    NATURAL_STRUCTURE_POWER,
    NATURAL_STRUCTURE_TEMPLATE
} NaturalStructureKind;

typedef struct NaturalStructure {
    u8 node;
    u8 extentStart;
    u8 extentEnd;
    u8 slotCount;
    NaturalStructureKind kind;
    NaturalSlot slots[NATURAL_MAX_SLOTS];
    u8 starts[NATURAL_MAX_SLOTS];
    u8 ends[NATURAL_MAX_SLOTS];
} NaturalStructure;

typedef struct NaturalTemplateSpec {
    const char *name;
    u8 slotCount;
    NaturalSlot slots[NATURAL_MAX_SLOTS];
} NaturalTemplateSpec;

static const NaturalTemplateSpec naturalTemplates[] = {
    {"sqrt", 1, {NATURAL_SLOT_RADICAND, NATURAL_SLOT_MAIN,
                  NATURAL_SLOT_MAIN, NATURAL_SLOT_MAIN}},
    {"root", 2, {NATURAL_SLOT_INDEX, NATURAL_SLOT_RADICAND,
                  NATURAL_SLOT_MAIN, NATURAL_SLOT_MAIN}},
    {"nroot", 2, {NATURAL_SLOT_INDEX, NATURAL_SLOT_RADICAND,
                   NATURAL_SLOT_MAIN, NATURAL_SLOT_MAIN}},
    {"sum", 4, {NATURAL_SLOT_BODY, NATURAL_SLOT_VARIABLE,
                 NATURAL_SLOT_LOWER, NATURAL_SLOT_UPPER}},
    {"prod", 4, {NATURAL_SLOT_BODY, NATURAL_SLOT_VARIABLE,
                  NATURAL_SLOT_LOWER, NATURAL_SLOT_UPPER}},
    {"integral", 4, {NATURAL_SLOT_BODY, NATURAL_SLOT_VARIABLE,
                      NATURAL_SLOT_LOWER, NATURAL_SLOT_UPPER}},
    {"d/dx", 3, {NATURAL_SLOT_BODY, NATURAL_SLOT_VARIABLE,
                  NATURAL_SLOT_EVALUATION, NATURAL_SLOT_MAIN}},
    {"d2/dx2", 3, {NATURAL_SLOT_BODY, NATURAL_SLOT_VARIABLE,
                    NATURAL_SLOT_EVALUATION, NATURAL_SLOT_MAIN}},
    {"diff", 3, {NATURAL_SLOT_BODY, NATURAL_SLOT_VARIABLE,
                  NATURAL_SLOT_EVALUATION, NATURAL_SLOT_MAIN}},
    {"deriv", 3, {NATURAL_SLOT_BODY, NATURAL_SLOT_VARIABLE,
                   NATURAL_SLOT_EVALUATION, NATURAL_SLOT_MAIN}}
};

static u8 naturalIsSpace(char value)
{
    return value == ' ' || value == '\t' || value == '\r' ||
           value == '\n';
}

static u8 naturalIsNameCharacter(char value)
{
    return (value >= '0' && value <= '9') ||
           (value >= 'A' && value <= 'Z') ||
           (value >= 'a' && value <= 'z') || value == '_';
}

static u8 naturalIsSeparator(char value)
{
    return value == ',' || value == ';';
}

static u8 naturalIsBinaryBoundary(char value)
{
    return value == '+' || value == '-' || value == '*' || value == '/' ||
           value == '=' || value == '<' || value == '>' ||
           naturalIsSeparator(value);
}

static u8 naturalIsUnarySign(const char *text, u8 signIndex)
{
    u8 index;
    char previous;

    index = signIndex;
    while (index != 0 && naturalIsSpace(text[index - 1]))
        index--;
    if (index == 0)
        return 1;
    previous = text[index - 1];
    return previous == '(' || previous == '^' ||
           naturalIsBinaryBoundary(previous);
}

static u8 naturalTemplateAt(const char *text, u8 openIndex,
                            u8 *nameStart, u8 *specIndex)
{
    u8 index;

    for (index = 0;
         index < (u8)(sizeof(naturalTemplates) / sizeof(naturalTemplates[0]));
         index++)
    {
        size_t nameLength;
        u8 start;

        nameLength = strlen(naturalTemplates[index].name);
        if (nameLength > openIndex)
            continue;
        start = (u8)(openIndex - nameLength);
        if (memcmp(text + start, naturalTemplates[index].name,
                   nameLength) != 0)
            continue;
        if (start != 0 && naturalIsNameCharacter(text[start - 1]))
            continue;
        *nameStart = start;
        *specIndex = index;
        return 1;
    }
    return 0;
}

static u8 naturalFindClose(const char *text, u8 length, u8 openIndex)
{
    u8 depth;
    u8 index;

    depth = 1;
    for (index = (u8)(openIndex + 1); index < length; index++)
    {
        if (text[index] == '(')
            depth++;
        else if (text[index] == ')')
        {
            depth--;
            if (depth == 0)
                return index;
        }
    }
    return length;
}

static void naturalAddTemplates(const char *text, u8 length,
                                NaturalStructure *structures,
                                u8 *structureCount, u8 *protectedText)
{
    u8 openIndex;

    for (openIndex = 0; openIndex < length; openIndex++)
    {
        const NaturalTemplateSpec *spec;
        NaturalStructure *structure;
        u8 nameStart;
        u8 specIndex;
        u8 closeIndex;
        u8 depth;
        u8 sourceIndex;
        u8 slotIndex;

        if (text[openIndex] != '(' ||
            !naturalTemplateAt(text, openIndex, &nameStart, &specIndex))
            continue;

        spec = &naturalTemplates[specIndex];
        closeIndex = naturalFindClose(text, length, openIndex);
        for (sourceIndex = nameStart; sourceIndex < openIndex; sourceIndex++)
            protectedText[sourceIndex] = 1;

        if (*structureCount == NATURAL_MAX_STRUCTURES)
            continue;
        structure = &structures[(*structureCount)++];
        memset(structure, 0, sizeof(*structure));
        structure->node = openIndex;
        structure->kind = NATURAL_STRUCTURE_TEMPLATE;
        structure->extentStart = nameStart;
        structure->extentEnd = closeIndex;
        structure->slotCount = 1;
        structure->slots[0] = spec->slots[0];
        structure->starts[0] = (u8)(openIndex + 1);

        depth = 0;
        slotIndex = 0;
        for (sourceIndex = (u8)(openIndex + 1);
             sourceIndex < closeIndex; sourceIndex++)
        {
            if (text[sourceIndex] == '(')
                depth++;
            else if (text[sourceIndex] == ')' && depth != 0)
                depth--;
            else if (depth == 0 && naturalIsSeparator(text[sourceIndex]) &&
                     slotIndex + 1 < spec->slotCount)
            {
                structure->ends[slotIndex] = sourceIndex;
                slotIndex++;
                structure->slotCount = (u8)(slotIndex + 1);
                structure->slots[slotIndex] = spec->slots[slotIndex];
                structure->starts[slotIndex] = (u8)(sourceIndex + 1);
            }
        }
        structure->ends[slotIndex] = closeIndex;
    }
}

static u8 naturalFindLeftOperand(const char *text, u8 operatorIndex,
                                 u8 *operandStart)
{
    u8 index;
    u8 start;
    u8 depth;
    u8 hasContent;

    if (operatorIndex == 0)
        return 0;
    index = operatorIndex;
    start = operatorIndex;
    depth = 0;
    hasContent = 0;
    while (index != 0)
    {
        char value;

        index--;
        value = text[index];
        if (value == ')')
        {
            depth++;
            start = index;
            hasContent = 1;
        }
        else if (value == '(')
        {
            if (depth == 0)
                break;
            depth--;
            start = index;
            hasContent = 1;
        }
        else if (depth == 0 && naturalIsBinaryBoundary(value))
        {
            if ((value == '+' || value == '-') &&
                naturalIsUnarySign(text, index))
            {
                start = index;
                hasContent = 1;
            }
            else
                break;
        }
        else
        {
            start = index;
            if (!naturalIsSpace(value))
                hasContent = 1;
        }
    }
    if (!hasContent)
        return 0;
    while (start < operatorIndex && naturalIsSpace(text[start]))
        start++;
    if (start == operatorIndex)
        return 0;
    *operandStart = start;
    return 1;
}

static u8 naturalFindRightOperand(const char *text, u8 length,
                                  u8 operatorIndex, u8 *operandEnd)
{
    u8 index;
    u8 depth;
    u8 hasContent;

    if (operatorIndex + 1 >= length)
        return 0;
    index = (u8)(operatorIndex + 1);
    depth = 0;
    hasContent = 0;
    while (index < length)
    {
        char value;

        value = text[index];
        if (value == '(')
        {
            depth++;
            hasContent = 1;
        }
        else if (value == ')')
        {
            if (depth == 0)
                break;
            depth--;
            hasContent = 1;
        }
        else if (depth == 0 && naturalIsBinaryBoundary(value))
        {
            if (!hasContent && (value == '+' || value == '-') &&
                naturalIsUnarySign(text, index))
                hasContent = 1;
            else
                break;
        }
        else if (!naturalIsSpace(value))
            hasContent = 1;
        index++;
    }
    while (index > operatorIndex + 1 && naturalIsSpace(text[index - 1]))
        index--;
    if (!hasContent || index == operatorIndex + 1)
        return 0;
    *operandEnd = index;
    return 1;
}

static void naturalAddBinary(const char *text, u8 length,
                             NaturalStructure *structures,
                             u8 *structureCount, const u8 *protectedText)
{
    u8 index;

    for (index = 0; index < length; index++)
    {
        NaturalStructure *structure;
        u8 leftStart;
        u8 rightEnd;

        if ((text[index] != '/' && text[index] != '^') ||
            protectedText[index])
            continue;
        if (!naturalFindLeftOperand(text, index, &leftStart) ||
            !naturalFindRightOperand(text, length, index, &rightEnd))
            continue;
        if (*structureCount == NATURAL_MAX_STRUCTURES)
            return;

        structure = &structures[(*structureCount)++];
        memset(structure, 0, sizeof(*structure));
        structure->node = index;
        structure->extentStart = leftStart;
        structure->extentEnd = rightEnd;
        structure->slotCount = 2;
        structure->starts[0] = leftStart;
        structure->ends[0] = index;
        structure->starts[1] = (u8)(index + 1);
        structure->ends[1] = rightEnd;
        if (text[index] == '/')
        {
            structure->kind = NATURAL_STRUCTURE_FRACTION;
            structure->slots[0] = NATURAL_SLOT_NUMERATOR;
            structure->slots[1] = NATURAL_SLOT_DENOMINATOR;
        }
        else
        {
            structure->kind = NATURAL_STRUCTURE_POWER;
            structure->slots[0] = NATURAL_SLOT_BASE;
            structure->slots[1] = NATURAL_SLOT_EXPONENT;
        }
    }
}

static u8 naturalBuildStructures(const char *text, u8 length,
                                 NaturalStructure *structures)
{
    u8 protectedText[NATURAL_MAX_TEXT + 1];
    u8 count;

    memset(protectedText, 0, sizeof(protectedText));
    count = 0;
    naturalAddTemplates(text, length, structures, &count, protectedText);
    naturalAddBinary(text, length, structures, &count, protectedText);
    return count;
}

static u8 naturalFindBestSlot(const NaturalStructure *structures,
                              u8 structureCount, u8 offset,
                              u8 *bestStructure, u8 *bestSlot)
{
    u8 structureIndex;
    u8 found;
    u8 bestWidth;
    u8 bestExtent;
    u8 bestDepth;

    found = 0;
    bestWidth = 255;
    bestExtent = 255;
    bestDepth = 0;
    for (structureIndex = 0; structureIndex < structureCount;
         structureIndex++)
    {
        const NaturalStructure *structure;
        u8 slotIndex;

        structure = &structures[structureIndex];
        for (slotIndex = 0; slotIndex < structure->slotCount; slotIndex++)
        {
            u8 width;
            u8 extent;

            if (offset < structure->starts[slotIndex] ||
                offset > structure->ends[slotIndex])
                continue;
            width = (u8)(structure->ends[slotIndex] -
                         structure->starts[slotIndex]);
            extent = (u8)(structure->extentEnd - structure->extentStart);
            if (!found || width < bestWidth ||
                (width == bestWidth && extent < bestExtent) ||
                (width == bestWidth && extent == bestExtent &&
                 structure->extentStart >= bestDepth))
            {
                found = 1;
                bestWidth = width;
                bestExtent = extent;
                bestDepth = structure->extentStart;
                *bestStructure = structureIndex;
                *bestSlot = slotIndex;
            }
            /* A separator boundary belongs to the slot on its left. */
            break;
        }
    }
    return found;
}

void naturalCursorSetEnd(NaturalCursor *cursor, u8 length)
{
    if (cursor == NULL)
        return;
    cursor->node = 0;
    cursor->slot = NATURAL_SLOT_MAIN;
    cursor->offset = length;
}

void naturalCursorRecompute(const char *text, u8 length,
                            NaturalCursor *cursor)
{
    NaturalStructure structures[NATURAL_MAX_STRUCTURES];
    u8 structureCount;
    u8 structureIndex;
    u8 slotIndex;

    if (cursor == NULL)
        return;
    if (text == NULL)
    {
        naturalCursorSetEnd(cursor, 0);
        return;
    }
    if (length > NATURAL_MAX_TEXT)
        length = NATURAL_MAX_TEXT;
    if (cursor->offset > length)
        cursor->offset = length;

    cursor->node = 0;
    cursor->slot = NATURAL_SLOT_MAIN;
    structureCount = naturalBuildStructures(text, length, structures);
    if (naturalFindBestSlot(structures, structureCount, cursor->offset,
                            &structureIndex, &slotIndex))
    {
        cursor->node = structures[structureIndex].node;
        cursor->slot = structures[structureIndex].slots[slotIndex];
    }
}

void naturalCursorMoveHorizontal(const char *text, u8 length,
                                 NaturalCursor *cursor, s8 direction)
{
    if (cursor == NULL || text == NULL || direction == 0)
        return;
    if (cursor->offset > length)
        cursor->offset = length;
    if (direction < 0)
    {
        if (cursor->offset == 0)
            return;
        cursor->offset--;
    }
    else
    {
        if (cursor->offset >= length)
            return;
        cursor->offset++;
    }
    naturalCursorRecompute(text, length, cursor);
}

void naturalCursorMoveVertical(const char *text, u8 length,
                               NaturalCursor *cursor, s8 direction)
{
    NaturalStructure structures[NATURAL_MAX_STRUCTURES];
    u8 structureCount;
    u8 structureIndex;
    u8 slotIndex;
    u8 targetSlot;
    u8 column;
    u8 targetWidth;

    if (cursor == NULL || text == NULL || direction == 0)
        return;
    if (length > NATURAL_MAX_TEXT)
        length = NATURAL_MAX_TEXT;
    if (cursor->offset > length)
        cursor->offset = length;

    naturalCursorRecompute(text, length, cursor);
    structureCount = naturalBuildStructures(text, length, structures);
    if (!naturalFindBestSlot(structures, structureCount, cursor->offset,
                             &structureIndex, &slotIndex))
        return;
    if (structures[structureIndex].node != cursor->node ||
        structures[structureIndex].slots[slotIndex] != cursor->slot)
        return;
    if ((direction < 0 && slotIndex == 0) ||
        (direction > 0 &&
         slotIndex + 1 >= structures[structureIndex].slotCount))
        return;

    targetSlot = direction < 0 ? (u8)(slotIndex - 1) :
                                 (u8)(slotIndex + 1);
    column = (u8)(cursor->offset -
                  structures[structureIndex].starts[slotIndex]);
    targetWidth = (u8)(structures[structureIndex].ends[targetSlot] -
                       structures[structureIndex].starts[targetSlot]);
    if (column > targetWidth)
        column = targetWidth;
    cursor->offset = (u8)(structures[structureIndex].starts[targetSlot] +
                          column);
    naturalCursorRecompute(text, length, cursor);
}

u8 naturalCursorInsert(char *text, u8 *length, u8 capacity,
                       const char *insert, NaturalCursor *cursor)
{
    size_t insertLength;
    u8 offset;

    if (text == NULL || length == NULL || insert == NULL || cursor == NULL)
        return 0;
    if (*length > capacity)
        return 0;
    insertLength = strlen(insert);
    if (insertLength == 0 ||
        insertLength > (size_t)(capacity - *length))
        return 0;

    offset = cursor->offset > *length ? *length : cursor->offset;
    memmove(text + offset + insertLength, text + offset,
            (size_t)(*length - offset) + 1);
    memcpy(text + offset, insert, insertLength);
    *length = (u8)(*length + insertLength);
    cursor->offset = (u8)(offset + insertLength);
    naturalCursorRecompute(text, *length, cursor);
    return 1;
}

u8 naturalCursorBackspace(char *text, u8 *length,
                          NaturalCursor *cursor)
{
    u8 offset;

    if (text == NULL || length == NULL || cursor == NULL || *length == 0)
        return 0;
    offset = cursor->offset > *length ? *length : cursor->offset;
    if (offset == 0)
        return 0;

    memmove(text + offset - 1, text + offset,
            (size_t)(*length - offset) + 1);
    (*length)--;
    cursor->offset = (u8)(offset - 1);
    naturalCursorRecompute(text, *length, cursor);
    return 1;
}
