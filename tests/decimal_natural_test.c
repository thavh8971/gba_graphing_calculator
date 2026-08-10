#include <stdio.h>
#include <string.h>

#include "gcalc/decimal.h"
#include "gcalc/natural.h"

static int failures;

static void failText(const char *name, const char *actual,
                     const char *expected)
{
    printf("FAIL %s: got [%s], expected [%s]\n", name, actual, expected);
    failures++;
}

static void expectDecimal(const char *name, DecimalNumber value,
                          const char *expected)
{
    char buffer[96];

    decimalFormat(value, buffer, (u8)sizeof(buffer), 30);
    if (strcmp(buffer, expected) != 0)
        failText(name, buffer, expected);
}

static DecimalNumber parseDecimal(const char *name, const char *text)
{
    DecimalNumber value;
    u8 error;

    value = decimalFromString(text, &error);
    if (error != CALC_OK)
    {
        printf("FAIL %s: parse error %u\n", name, error);
        failures++;
    }
    return value;
}

static void expectDivide(const char *name, const char *numerator,
                         const char *denominator, const char *expected)
{
    DecimalNumber result;
    u8 error;

    result = decimalDivide(parseDecimal(name, numerator),
                           parseDecimal(name, denominator), &error);
    if (error != CALC_OK)
    {
        printf("FAIL %s: divide error %u\n", name, error);
        failures++;
        return;
    }
    expectDecimal(name, result, expected);
}

static void expectSlot(const char *name, const char *text, u8 offset,
                       NaturalSlot expected)
{
    NaturalCursor cursor;

    cursor.node = 0;
    cursor.slot = NATURAL_SLOT_MAIN;
    cursor.offset = offset;
    naturalCursorRecompute(text, (u8)strlen(text), &cursor);
    if (cursor.slot != expected)
    {
        printf("FAIL %s: got slot %d, expected %d (offset %u)\n",
               name, (int)cursor.slot, (int)expected, cursor.offset);
        failures++;
    }
}

static void expectVertical(const char *name, const char *text, u8 offset,
                           s8 direction, NaturalSlot expectedSlot,
                           u8 expectedOffset)
{
    NaturalCursor cursor;

    cursor.node = 0;
    cursor.slot = NATURAL_SLOT_MAIN;
    cursor.offset = offset;
    naturalCursorRecompute(text, (u8)strlen(text), &cursor);
    naturalCursorMoveVertical(text, (u8)strlen(text), &cursor, direction);
    if (cursor.slot != expectedSlot || cursor.offset != expectedOffset)
    {
        printf("FAIL %s: got slot %d offset %u, expected %d offset %u\n",
               name, (int)cursor.slot, cursor.offset, (int)expectedSlot,
               expectedOffset);
        failures++;
    }
}

static void testDecimal(void)
{
    DecimalNumber harmonic;
    DecimalNumber term;
    DecimalNumber value;
    u8 error;
    u16 index;

    expectDecimal("integer", decimalFromS32(100), "100");
    expectDecimal("s32 minimum", decimalFromS32((s32)0x80000000UL),
                  "-2147483648");
    expectDecimal("decimal comma", parseDecimal("decimal comma", "-0,00123"),
                  "-0.00123");
    expectDecimal("scientific parse", parseDecimal("scientific", "1.25E3"),
                  "1250");
    expectDecimal("add", decimalAdd(parseDecimal("add left", "1.2"),
                                     parseDecimal("add right", "3.45")),
                  "4.65");
    expectDecimal("subtract",
                  decimalSubtract(parseDecimal("sub left", "1"),
                                  parseDecimal("sub right", "0.125")),
                  "0.875");
    expectDecimal("negative subtract",
                  decimalSubtract(parseDecimal("negative sub left", "1"),
                                  parseDecimal("negative sub right", "2")),
                  "-1");
    expectDecimal("multiply",
                  decimalMultiply(parseDecimal("mul left", "1.2"),
                                  parseDecimal("mul right", "1.2")),
                  "1.44");

    expectDivide("one third", "1", "3",
                 "0.333333333333333333333333333333");
    expectDivide("one half", "1", "2", "0.5");
    expectDivide("one tenth", "1", "10", "0.1");
    expectDivide("one eleventh", "1", "11",
                 "0.0909090909090909090909090909091");
    expectDivide("one hundredth", "1", "100", "0.01");

    value = decimalDivide(decimalFromS32(1), decimalZero(), &error);
    (void)value;
    if (error != CALC_ERR_DIVZERO)
    {
        printf("FAIL divide by zero: got error %u\n", error);
        failures++;
    }

    harmonic = decimalZero();
    for (index = 1; index <= 1000; index++)
    {
        term = decimalDivide(decimalFromS32(1), decimalFromS32(index),
                             &error);
        if (error != CALC_OK)
        {
            printf("FAIL H_1000 division at %u: error %u\n", index, error);
            failures++;
            break;
        }
        harmonic = decimalAdd(harmonic, term);
    }
    expectDecimal("H_1000", harmonic,
                  "7.48547086055034491265651820433");
}

static void testNaturalSlots(void)
{
    NaturalCursor cursor;
    char edit[16];
    u8 length;

    expectSlot("fraction numerator", "1/2", 1, NATURAL_SLOT_NUMERATOR);
    expectSlot("fraction denominator", "1/2", 3,
               NATURAL_SLOT_DENOMINATOR);
    expectVertical("fraction down", "1/2", 1, 1,
                   NATURAL_SLOT_DENOMINATOR, 3);
    expectVertical("fraction up", "1/2", 3, -1,
                   NATURAL_SLOT_NUMERATOR, 1);

    expectSlot("power exponent", "x^2+y^2", 6, NATURAL_SLOT_EXPONENT);
    expectVertical("power to base", "x^2+y^2", 6, -1,
                   NATURAL_SLOT_BASE, 4);
    expectSlot("fraction stops at plus", "1/2+3", 3,
               NATURAL_SLOT_DENOMINATOR);

    expectSlot("sqrt radicand", "sqrt(x+1)", 7, NATURAL_SLOT_RADICAND);
    expectSlot("incomplete sqrt", "sqrt(", 5, NATURAL_SLOT_RADICAND);
    expectSlot("root index", "root(3;8)", 5, NATURAL_SLOT_INDEX);
    expectSlot("root radicand", "root(3;8)", 8, NATURAL_SLOT_RADICAND);
    expectVertical("root to index", "root(3;8)", 7, -1,
                   NATURAL_SLOT_INDEX, 5);

    expectSlot("sum body", "sum(x;x;1;3)", 4, NATURAL_SLOT_BODY);
    expectSlot("sum variable", "sum(x;x;1;3)", 6,
               NATURAL_SLOT_VARIABLE);
    expectSlot("sum lower", "sum(x;x;1;3)", 8, NATURAL_SLOT_LOWER);
    expectSlot("sum upper", "sum(x;x;1;3)", 10, NATURAL_SLOT_UPPER);
    expectVertical("sum body to variable", "sum(x;x;1;3)", 4, 1,
                   NATURAL_SLOT_VARIABLE, 6);
    expectSlot("product upper", "prod(x;x;1;4)", 11,
               NATURAL_SLOT_UPPER);
    expectSlot("integral lower", "integral(x;x;0;1)", 13,
               NATURAL_SLOT_LOWER);
    expectSlot("derivative variable", "d/dx(x;x;3)", 7,
               NATURAL_SLOT_VARIABLE);
    expectSlot("derivative evaluation", "d/dx(x;x;3)", 9,
               NATURAL_SLOT_EVALUATION);
    expectSlot("second derivative body", "d2/dx2(x;x;3)", 7,
               NATURAL_SLOT_BODY);
    expectSlot("nested fraction", "sum(1/x;x;1;3)", 6,
               NATURAL_SLOT_DENOMINATOR);

    strcpy(edit, "12");
    length = 2;
    naturalCursorSetEnd(&cursor, length);
    cursor.offset = 1;
    naturalCursorRecompute(edit, length, &cursor);
    if (!naturalCursorInsert(edit, &length, 15, "+", &cursor) ||
        strcmp(edit, "1+2") != 0 || length != 3 || cursor.offset != 2)
    {
        printf("FAIL natural insert: text=%s length=%u offset=%u\n",
               edit, length, cursor.offset);
        failures++;
    }
    if (!naturalCursorBackspace(edit, &length, &cursor) ||
        strcmp(edit, "12") != 0 || length != 2 || cursor.offset != 1)
    {
        printf("FAIL natural backspace: text=%s length=%u offset=%u\n",
               edit, length, cursor.offset);
        failures++;
    }

    strcpy(edit, "1/2");
    length = 3;
    cursor.offset = 1;
    naturalCursorRecompute(edit, length, &cursor);
    naturalCursorMoveHorizontal(edit, length, &cursor, 1);
    if (cursor.offset != 2 || cursor.slot != NATURAL_SLOT_DENOMINATOR)
    {
        printf("FAIL natural horizontal: slot=%d offset=%u\n",
               (int)cursor.slot, cursor.offset);
        failures++;
    }
}

int main(void)
{
    testDecimal();
    testNaturalSlots();
    if (failures != 0)
        return 1;
    puts("decimal_natural_test: PASS");
    return 0;
}
