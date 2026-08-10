#include <stdio.h>
#include <string.h>

#include "gcalc/app.h"

/* Keep padding between visible scanlines so this test catches a renderer
   which accidentally assumes a packed Mode 3 surface. */
#define TEST_STRIDE (GBA_SCREEN_WIDTH + 9)
#define TEST_GUARD_WORDS 37
#define TEST_VISIBLE_WORDS (TEST_STRIDE * GBA_SCREEN_HEIGHT)
#define TEST_SENTINEL 0x5aa5

#define TEST_COLOR_TEXT RGB15(29, 30, 31)
#define TEST_COLOR_MUTED RGB15(16, 19, 22)

#define MATRIX_LEFT_OUTER 7
#define MATRIX_LEFT_INNER 8
#define MATRIX_RIGHT_INNER 231
#define MATRIX_RIGHT_OUTER 232
#define MATRIX_CAP_LENGTH 6
#define MATRIX_CONTENT_LEFT 14
#define MATRIX_CONTENT_RIGHT 226

static u16 storage[TEST_GUARD_WORDS + TEST_VISIBLE_WORDS +
                   TEST_GUARD_WORDS];
#define framebuffer (storage + TEST_GUARD_WORDS)

static int failures;

static void check(int condition, const char *message)
{
    if (!condition) {
        fprintf(stderr, "FAIL matrix/vector UI: %s\n", message);
        failures++;
    }
}

static u16 pixelAt(s16 x, s16 y)
{
    return framebuffer[(u32)y * TEST_STRIDE + (u32)x];
}

static void initializeFramebuffer(void)
{
    u32 index;

    for (index = 0; index < GCALC_ARRAY_COUNT(storage); index++)
        storage[index] = TEST_SENTINEL;
}

static void render(AppState *app)
{
    GfxSurface surface;

    surface.pixels = framebuffer;
    surface.width = GBA_SCREEN_WIDTH;
    surface.height = GBA_SCREEN_HEIGHT;
    surface.stride = TEST_STRIDE;
    app->dirty = APP_DIRTY_ALL;
    appRender(app, &surface);
}

static void checkFramebufferBounds(void)
{
    u32 index;
    u16 y;
    u8 intact = 1;

    for (index = 0; index < TEST_GUARD_WORDS; index++) {
        if (storage[index] != TEST_SENTINEL ||
            storage[TEST_GUARD_WORDS + TEST_VISIBLE_WORDS + index] !=
                TEST_SENTINEL) {
            intact = 0;
            break;
        }
    }
    check(intact, "large-bracket rendering preserves outer guard words");

    intact = 1;
    for (y = 0; y < GBA_SCREEN_HEIGHT && intact; y++) {
        u16 x;
        for (x = GBA_SCREEN_WIDTH; x < TEST_STRIDE; x++) {
            if (pixelAt((s16)x, (s16)y) != TEST_SENTINEL) {
                intact = 0;
                break;
            }
        }
    }
    check(intact, "large-bracket rendering preserves scanline padding");
}

static void pressKey(AppState *app, u16 keys)
{
    appHandleKeys(app, keys, keys);
}

static void enterActionMenu(AppState *app, CalcMode mode)
{
    appInit(app);
    pressKey(app, APP_KEY_SELECT | APP_KEY_START);
    check(app->view == APP_VIEW_MENU, "mode menu opens");
    app->menuSelection = (u8)mode;
    pressKey(app, APP_KEY_A);
    check(app->mode == mode && app->view == APP_VIEW_MODE_ACTION,
          "MATRIX/VECTOR enters its action selector");
}

static void openAction(AppState *app, u8 action, AppView expectedView)
{
    app->actionSelection = action;
    pressKey(app, APP_KEY_A);
    check(app->view == expectedView,
          "MATRIX/VECTOR action opens its owned workspace");
}

static void enterGrid(AppState *app, CalcMode mode, u8 action)
{
    enterActionMenu(app, mode);
    openAction(app, action, APP_VIEW_MODE_GRID);
}

static void setGridCell(AppState *app, u8 row, u8 column,
                        const char *text)
{
    size_t length = strlen(text);

    check(length <= APP_GRID_CELL_CAPACITY,
          "test grid expression fits one cell");
    if (length > APP_GRID_CELL_CAPACITY)
        length = APP_GRID_CELL_CAPACITY;
    memcpy(app->gridCells[0][row][column], text, length);
    app->gridCells[0][row][column][length] = '\0';
    app->gridCellLengths[0][row][column] = (u8)length;
}

static u8 rectangleHasColor(s16 left, s16 top, s16 right, s16 bottom,
                            u16 color)
{
    s16 y;

    for (y = top; y <= bottom; y++) {
        s16 x;
        for (x = left; x <= right; x++)
            if (pixelAt(x, y) != color)
                return 0;
    }
    return 1;
}

static void checkLargeBrackets(s16 top, s16 bottom, const char *message)
{
    u8 valid = 1;

    valid = valid && rectangleHasColor(MATRIX_LEFT_OUTER, top,
                                       MATRIX_LEFT_INNER, bottom,
                                       TEST_COLOR_TEXT);
    valid = valid && rectangleHasColor(MATRIX_RIGHT_INNER, top,
                                       MATRIX_RIGHT_OUTER, bottom,
                                       TEST_COLOR_TEXT);
    valid = valid && rectangleHasColor(MATRIX_LEFT_OUTER, top,
                                       MATRIX_LEFT_OUTER +
                                           MATRIX_CAP_LENGTH - 1,
                                       (s16)(top + 1), TEST_COLOR_TEXT);
    valid = valid && rectangleHasColor(MATRIX_LEFT_OUTER,
                                       (s16)(bottom - 1),
                                       MATRIX_LEFT_OUTER +
                                           MATRIX_CAP_LENGTH - 1,
                                       bottom, TEST_COLOR_TEXT);
    valid = valid && rectangleHasColor(MATRIX_RIGHT_OUTER -
                                           MATRIX_CAP_LENGTH + 1,
                                       top, MATRIX_RIGHT_OUTER,
                                       (s16)(top + 1), TEST_COLOR_TEXT);
    valid = valid && rectangleHasColor(MATRIX_RIGHT_OUTER -
                                           MATRIX_CAP_LENGTH + 1,
                                       (s16)(bottom - 1),
                                       MATRIX_RIGHT_OUTER, bottom,
                                       TEST_COLOR_TEXT);
    check(valid, message);
}

static u8 glyphHasColor(s16 x, s16 y, char glyph, u16 color)
{
    u8 row;

    for (row = 0; row < GFX_ASCII5X7_HEIGHT; row++) {
        u8 bits = gfxGlyph5x7Row(glyph, row);
        u8 column;
        for (column = 0; column < GFX_ASCII5X7_WIDTH; column++) {
            if ((bits & (1u << (GFX_ASCII5X7_WIDTH - 1 - column))) != 0 &&
                pixelAt((s16)(x + column), (s16)(y + row)) != color)
                return 0;
        }
    }
    return 1;
}

static s16 centeredGlyphX(u8 column, u8 columns)
{
    s16 span = MATRIX_CONTENT_RIGHT - MATRIX_CONTENT_LEFT;
    s16 left = (s16)(MATRIX_CONTENT_LEFT +
                     ((s16)column * span) / columns);
    s16 right = (s16)(MATRIX_CONTENT_LEFT +
                      ((s16)(column + 1) * span) / columns);

    return (s16)(left + (right - left - GFX_ASCII5X7_WIDTH) / 2);
}

static void checkEmptyZeroLayout(u8 rows, u8 columns, s16 blockTop,
                                 s16 rowHeight, const char *message)
{
    u8 valid = 1;
    u8 row;

    for (row = 0; row < rows; row++) {
        u8 column;
        s16 y = (s16)(blockTop + row * rowHeight +
                      (rowHeight - GFX_ASCII5X7_HEIGHT) / 2);
        for (column = 0; column < columns; column++) {
            s16 x = centeredGlyphX(column, columns);
            if (!glyphHasColor(x, y, '0', TEST_COLOR_MUTED))
                valid = 0;
        }
    }
    check(valid, message);
}

static void testMatrixFourByFour(void)
{
    AppState app;
    ModeMatrixRegister stored;

    enterGrid(&app, CALC_MODE_MATRIX, 0);
    check(app.gridRows[0] == 4 && app.gridColumns[0] == 4,
          "undefined MatA opens as a 4x4 register");

    initializeFramebuffer();
    render(&app);
    checkLargeBrackets(28, 67,
        "4x4 MATRIX uses a 40-pixel, two-pixel-thick square bracket");
    checkEmptyZeroLayout(4, 4, 28, 10,
        "empty 4x4 MATRIX cells show sixteen aligned muted zeros");
    checkFramebufferBounds();

    /* Rendering the bracket must not turn D-pad cell navigation into keypad
       navigation or alter the active cell's editor. */
    pressKey(&app, APP_KEY_SELECT | APP_KEY_RIGHT);
    pressKey(&app, APP_KEY_SELECT | APP_KEY_DOWN);
    check(app.gridPanel == 0 && app.gridRow == 1 && app.gridColumn == 1,
          "large MATRIX retains structural cell navigation");
    app.tokenPage = 0;
    app.selectedToken = 0;
    pressKey(&app, APP_KEY_A);
    check(strcmp(app.gridCells[0][1][1], "7") == 0 &&
          app.gridCellLengths[0][1][1] == 1,
          "large MATRIX retains selected-cell editing");
    pressKey(&app, APP_KEY_SELECT | APP_KEY_B);
    check(app.view == APP_VIEW_MODE_GRID &&
          app.gridCellLengths[0][1][1] == 0,
          "SELECT+B backspaces MATRIX without saving or leaving");
    pressKey(&app, APP_KEY_A);

    initializeFramebuffer();
    render(&app);
    checkLargeBrackets(28, 67,
        "4x4 MATRIX bracket survives navigation and editing redraws");
    checkFramebufferBounds();

    pressKey(&app, APP_KEY_B);
    check(app.view == APP_VIEW_MODE_ACTION &&
          strstr(app.status, "MatA SAVED") != 0,
          "B saves MatA and exits its register table");
    check(modeMatrixGetRegister(&app.runtime, MODE_REGISTER_A, &stored) &&
          stored.rows == 4 && stored.columns == 4 &&
          calcNumberToLongDouble(stored.cell[0][0]) == 0.0L &&
          calcNumberToLongDouble(stored.cell[1][1]) == 7.0L,
          "MatA stores edited values and converts empty cells to zero");

    openAction(&app, 0, APP_VIEW_MODE_GRID);
    check(app.gridRows[0] == 4 && app.gridColumns[0] == 4 &&
          strcmp(app.gridCells[0][0][0], "0") == 0 &&
          strcmp(app.gridCells[0][1][1], "7") == 0,
          "reopening MatA restores dimensions, zeros, and edited cells");
    pressKey(&app, APP_KEY_SELECT | APP_KEY_RIGHT);
    pressKey(&app, APP_KEY_SELECT | APP_KEY_DOWN);
    pressKey(&app, APP_KEY_SELECT | APP_KEY_B);
    check(app.gridCellLengths[0][1][1] == 0,
          "SELECT+B deletes a restored MatA value");
    pressKey(&app, APP_KEY_START);
    check(app.view == APP_VIEW_MODE_ACTION &&
          modeMatrixGetRegister(&app.runtime, MODE_REGISTER_A, &stored) &&
          calcNumberToLongDouble(stored.cell[1][1]) == 0.0L,
          "START saves the edited MatA and exits its table");
}

static void testVectorRow(void)
{
    AppState app;
    ModeVectorRegister stored;

    enterGrid(&app, CALC_MODE_VECTOR, 0);
    check(app.gridRows[0] == 1 && app.gridColumns[0] == 3,
          "undefined VctA opens a horizontal three-component row");
    initializeFramebuffer();
    render(&app);
    checkLargeBrackets(41, 53,
        "VECTOR row uses a centered 13-pixel large square bracket");
    /* The 10-pixel value row is vertically centered inside the 13-pixel
       bracket, so its content origin is y=42. */
    checkEmptyZeroLayout(1, 3, 42, 10,
        "empty VECTOR row shows three evenly aligned muted zeros");
    checkFramebufferBounds();

    pressKey(&app, APP_KEY_SELECT | APP_KEY_RIGHT);
    check(app.gridRow == 0 && app.gridColumn == 1,
          "VECTOR bracket retains component navigation");
    app.tokenPage = 0;
    app.selectedToken = 0;
    pressKey(&app, APP_KEY_A);
    check(strcmp(app.gridCells[0][0][1], "7") == 0,
          "VECTOR bracket retains component editing");
    pressKey(&app, APP_KEY_B);
    check(app.view == APP_VIEW_MODE_ACTION &&
          modeVectorGetRegister(&app.runtime, MODE_REGISTER_A, &stored) &&
          stored.dimensions == 3 &&
          calcNumberToLongDouble(stored.component[0]) == 0.0L &&
          calcNumberToLongDouble(stored.component[1]) == 7.0L,
          "B stores VctA, including empty components as zero, and exits");

    openAction(&app, 0, APP_VIEW_MODE_GRID);
    check(strcmp(app.gridCells[0][0][0], "0") == 0 &&
          strcmp(app.gridCells[0][0][1], "7") == 0,
          "reopening VctA restores its saved components");
    pressKey(&app, APP_KEY_SELECT | APP_KEY_RIGHT);
    pressKey(&app, APP_KEY_SELECT | APP_KEY_B);
    check(app.gridCellLengths[0][0][1] == 0,
          "SELECT+B deletes a restored VctA component");
    pressKey(&app, APP_KEY_START);
    check(app.view == APP_VIEW_MODE_ACTION &&
          modeVectorGetRegister(&app.runtime, MODE_REGISTER_A, &stored) &&
          calcNumberToLongDouble(stored.component[1]) == 0.0L,
          "START saves the edited VctA and exits its table");
}

static void testLargeMatrixCellWiseSave(void)
{
    static const char expression[] = "1+1+1+1+1+1+1";
    AppState app;
    ModeMatrixRegister stored;
    u8 row;
    u8 column;
    u16 aggregateLength = 0;
    u8 valuesValid = 1;

    enterGrid(&app, CALC_MODE_MATRIX, 0);
    for (row = 0; row < 4; row++) {
        for (column = 0; column < 4; column++) {
            setGridCell(&app, row, column, expression);
            aggregateLength = (u16)(aggregateLength +
                                     strlen(expression));
        }
    }
    check(aggregateLength > APP_EXPRESSION_CAPACITY,
          "4x4 test source exceeds the old 127-byte serialization limit");
    pressKey(&app, APP_KEY_B);
    check(app.view == APP_VIEW_MODE_ACTION &&
          app.runtime.error == CALC_OK &&
          modeMatrixGetRegister(&app.runtime, MODE_REGISTER_A, &stored),
          "B saves a 4x4 register without a whole-matrix source buffer");
    for (row = 0; row < 4; row++)
        for (column = 0; column < 4; column++)
            if (calcNumberToLongDouble(stored.cell[row][column]) != 7.0L)
                valuesValid = 0;
    check(valuesValid,
          "cell-wise save evaluates all sixteen long cell expressions");

    openAction(&app, 0, APP_VIEW_MODE_GRID);
    valuesValid = 1;
    for (row = 0; row < 4; row++)
        for (column = 0; column < 4; column++)
            if (strcmp(app.gridCells[0][row][column], "7") != 0)
                valuesValid = 0;
    check(valuesValid,
          "reopening MatA restores every value from the oversized input");

    setGridCell(&app, 2, 3, "1/0");
    pressKey(&app, APP_KEY_B);
    check(app.view == APP_VIEW_MODE_GRID &&
          app.runtime.error == CALC_ERR_DIVZERO &&
          app.gridRow == 2 && app.gridColumn == 3 &&
          strcmp(app.status, calcErrorText(CALC_ERR_DIVZERO)) == 0,
          "a bad cell keeps the editor open and focuses the reported error");
    check(modeMatrixGetRegister(&app.runtime, MODE_REGISTER_A, &stored) &&
          calcNumberToLongDouble(stored.cell[2][3]) == 7.0L,
          "failed cell evaluation leaves the stored register unchanged");
}

static void testActionMapsAndKeypads(void)
{
    static const AppFormAction matrixEdits[4] = {
        APP_FORM_MATRIX_EDIT_A, APP_FORM_MATRIX_EDIT_B,
        APP_FORM_MATRIX_EDIT_C, APP_FORM_MATRIX_EDIT_D
    };
    static const AppFormAction matrixOperations[6] = {
        APP_FORM_MATRIX_DET, APP_FORM_MATRIX_INV,
        APP_FORM_MATRIX_TRANSPOSE, APP_FORM_MATRIX_ADD,
        APP_FORM_MATRIX_SUB, APP_FORM_MATRIX_MUL
    };
    static const AppFormAction vectorEdits[4] = {
        APP_FORM_VECTOR_EDIT_A, APP_FORM_VECTOR_EDIT_B,
        APP_FORM_VECTOR_EDIT_C, APP_FORM_VECTOR_EDIT_D
    };
    static const AppFormAction vectorOperations[5] = {
        APP_FORM_VECTOR_NORM, APP_FORM_VECTOR_DOT,
        APP_FORM_VECTOR_CROSS, APP_FORM_VECTOR_ANGLE,
        APP_FORM_VECTOR_SCALE
    };
    AppState app;
    u8 index;

    check(appModeKeypadPageCount(CALC_MODE_MATRIX) == 3 &&
          appModeKeypadPageCount(CALC_MODE_VECTOR) == 3,
          "MATRIX and VECTOR each own three 6x5 keypad pages");

    for (index = 0; index < 4; index++) {
        enterGrid(&app, CALC_MODE_MATRIX, index);
        check(app.formAction == (u8)matrixEdits[index] &&
              app.gridRows[0] == 4 && app.gridColumns[0] == 4,
              "MATRIX actions 0..3 map to EDIT MatA..MatD");
        pressKey(&app, APP_KEY_B);

        enterGrid(&app, CALC_MODE_VECTOR, index);
        check(app.formAction == (u8)vectorEdits[index] &&
              app.gridRows[0] == 1 && app.gridColumns[0] == 3,
              "VECTOR actions 0..3 map to EDIT VctA..VctD");
        pressKey(&app, APP_KEY_B);
    }

    for (index = 0; index < 6; index++) {
        enterActionMenu(&app, CALC_MODE_MATRIX);
        openAction(&app, (u8)(index + 4), APP_VIEW_MODE_FORM);
        check(app.formAction == (u8)matrixOperations[index],
              "MATRIX actions 4..9 map to canonical operation forms");
    }
    for (index = 0; index < 5; index++) {
        enterActionMenu(&app, CALC_MODE_VECTOR);
        openAction(&app, (u8)(index + 4), APP_VIEW_MODE_FORM);
        check(app.formAction == (u8)vectorOperations[index],
              "VECTOR actions 4..8 map to canonical operation forms");
    }

    enterGrid(&app, CALC_MODE_MATRIX, 0);
    for (index = 0; index < 3; index++) {
        u8 oldRow = app.gridRow;
        u8 oldColumn = app.gridColumn;

        app.tokenPage = index;
        app.selectedToken = 29;
        initializeFramebuffer();
        render(&app);
        check(app.dirty == APP_DIRTY_NONE,
              "every MATRIX keypad page renders completely");
        checkFramebufferBounds();
        pressKey(&app, APP_KEY_A);
        check(app.gridRow != oldRow || app.gridColumn != oldColumn,
              "slot 29 is EXE, not text, on every MATRIX page");
    }

    enterGrid(&app, CALC_MODE_VECTOR, 0);
    for (index = 0; index < 3; index++) {
        u8 oldColumn = app.gridColumn;

        app.tokenPage = index;
        app.selectedToken = 29;
        pressKey(&app, APP_KEY_A);
        check(app.gridColumn != oldColumn,
              "slot 29 is EXE, not text, on every VECTOR page");
    }
}

static void testCanonicalOperationForms(void)
{
    AppState app;

    enterActionMenu(&app, CALC_MODE_MATRIX);
    check(modeMatrixSetRegisterExpression(&app.runtime, MODE_REGISTER_A,
                                          "[1,2;3,4]") == CALC_OK,
          "test setup stores MatA");
    openAction(&app, 4, APP_VIEW_MODE_FORM);
    check(strcmp(app.formFields[0], "MatA") == 0,
          "det() defaults to MatA");
    pressKey(&app, APP_KEY_START);
    check(app.runtime.error == CALC_OK &&
          strcmp(app.expression, "det(MatA)") == 0 &&
          strcmp(app.runtime.result, "-2") == 0,
          "START evaluates canonical det(MatA)");

    enterActionMenu(&app, CALC_MODE_MATRIX);
    check(modeMatrixSetRegisterExpression(&app.runtime, MODE_REGISTER_A,
                                          "[1,2;3,4]") == CALC_OK &&
          modeMatrixSetRegisterExpression(&app.runtime, MODE_REGISTER_B,
                                          "[4,3;2,1]") == CALC_OK,
          "test setup stores MatA and MatB");
    openAction(&app, 9, APP_VIEW_MODE_FORM);
    check(strcmp(app.formFields[0], "MatA") == 0 &&
          strcmp(app.formFields[1], "MatB") == 0,
          "mul() defaults to MatA and MatB");
    app.selectedToken = 29;
    pressKey(&app, APP_KEY_A);
    check(app.runtime.error == CALC_OK &&
          strcmp(app.expression, "mul(MatA;MatB)") == 0 &&
          strcmp(app.runtime.result, "[8,5;20,13]") == 0,
          "bottom-right EXE evaluates canonical mul(MatA;MatB)");

    enterActionMenu(&app, CALC_MODE_VECTOR);
    check(modeVectorSetRegisterExpression(&app.runtime, MODE_REGISTER_A,
                                          "[1,2,3]") == CALC_OK &&
          modeVectorSetRegisterExpression(&app.runtime, MODE_REGISTER_B,
                                          "[4,5,6]") == CALC_OK,
          "test setup stores VctA and VctB");
    openAction(&app, 5, APP_VIEW_MODE_FORM);
    check(strcmp(app.formFields[0], "VctA") == 0 &&
          strcmp(app.formFields[1], "VctB") == 0,
          "dot() defaults to VctA and VctB");
    pressKey(&app, APP_KEY_START);
    check(app.runtime.error == CALC_OK &&
          strcmp(app.expression, "dot(VctA;VctB)") == 0 &&
          strcmp(app.runtime.result, "32") == 0,
          "START evaluates canonical dot(VctA;VctB)");
}

int main(void)
{
    check(sizeof(AppState) <= 16u * 1024u,
          "register UI keeps AppState inside its 16 KiB memory budget");
    testActionMapsAndKeypads();
    testMatrixFourByFour();
    testVectorRow();
    testLargeMatrixCellWiseSave();
    testCanonicalOperationForms();

    if (failures != 0) {
        fprintf(stderr, "matrix_vector_ui_test: %d failure(s)\n", failures);
        return 1;
    }
    puts("matrix_vector_ui_test: PASS");
    return 0;
}
