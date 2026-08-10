#include <stdio.h>
#include <string.h>

#include "gcalc/app.h"

#define KEYPAD_COLUMNS 6
#define KEYPAD_ROWS 5
#define KEYPAD_EXE 29
#define FRAMEBUFFER_GUARD_WORDS 32
#define FRAMEBUFFER_WORDS (GBA_SCREEN_WIDTH * GBA_SCREEN_HEIGHT)
#define FRAMEBUFFER_SENTINEL 0x5aa5

static u16 framebufferStorage[FRAMEBUFFER_GUARD_WORDS +
                              FRAMEBUFFER_WORDS +
                              FRAMEBUFFER_GUARD_WORDS];
#define framebuffer (framebufferStorage + FRAMEBUFFER_GUARD_WORDS)

static int failures;

static void check(int condition, const char *message)
{
    if (!condition) {
        fprintf(stderr, "FAIL app workspace: %s\n", message);
        failures++;
    }
}

static void pressKey(AppState *app, u16 key)
{
    appHandleKeys(app, key, key);
}

static void render(AppState *app)
{
    GfxSurface surface;

    surface.pixels = framebuffer;
    surface.width = GBA_SCREEN_WIDTH;
    surface.height = GBA_SCREEN_HEIGHT;
    surface.stride = GBA_SCREEN_WIDTH;
    appRender(app, &surface);
}

static void initializeFramebuffer(void)
{
    u32 index;

    for (index = 0; index < GCALC_ARRAY_COUNT(framebufferStorage); index++)
        framebufferStorage[index] = FRAMEBUFFER_SENTINEL;
}

static void checkFramebufferGuards(void)
{
    u16 index;
    u8 intact = 1;

    for (index = 0; index < FRAMEBUFFER_GUARD_WORDS; index++) {
        if (framebufferStorage[index] != FRAMEBUFFER_SENTINEL ||
            framebufferStorage[FRAMEBUFFER_GUARD_WORDS +
                               FRAMEBUFFER_WORDS + index] !=
                FRAMEBUFFER_SENTINEL) {
            intact = 0;
            break;
        }
    }
    check(intact, "workspace rendering stays inside the framebuffer");
}

static void copyText(char *destination, u8 *length, u16 capacity,
                     const char *source)
{
    size_t count = strlen(source);

    if (count >= capacity)
        count = capacity - 1;
    memcpy(destination, source, count);
    destination[count] = '\0';
    *length = (u8)count;
}

static void enterMode(AppState *app, CalcMode mode)
{
    appInit(app);
    appHandleKeys(app, APP_KEY_SELECT | APP_KEY_START, 0);
    check(app->view == APP_VIEW_MENU, "SELECT+START opens MODE");
    app->menuSelection = (u8)mode;
    pressKey(app, APP_KEY_A);
    check(app->mode == mode && app->view == appModeEntryView(mode),
          "MODE selection routes to the mode-owned entry view");
}

static AppView beginDataWorkspace(AppState *app, CalcMode mode,
                                  u8 action)
{
    enterMode(app, mode);
    app->actionSelection = action;
    if (app->view == APP_VIEW_MODE_ACTION ||
        app->view == APP_VIEW_STAT_TYPE)
        pressKey(app, APP_KEY_A);
    return app->view;
}

static void clearCurrentEditor(AppState *app)
{
    switch (app->view) {
    case APP_VIEW_CALCULATOR:
        appSetExpression(app, "");
        break;
    case APP_VIEW_MODE_FORM:
        app->formFields[app->formField][0] = '\0';
        app->formLengths[app->formField] = 0;
        naturalCursorSetEnd(&app->formCursor, 0);
        break;
    case APP_VIEW_MODE_GRID:
        app->gridCells[app->gridPanel][app->gridRow]
                      [app->gridColumn][0] = '\0';
        app->gridCellLengths[app->gridPanel][app->gridRow]
                            [app->gridColumn] = 0;
        naturalCursorSetEnd(&app->gridCursor, 0);
        break;
    case APP_VIEW_STAT_DATA:
        app->statCells[app->statRow][app->statColumn][0] = '\0';
        app->statCellLengths[app->statRow][app->statColumn] = 0;
        naturalCursorSetEnd(&app->formCursor, 0);
        break;
    default:
        break;
    }
}

static u8 currentEditorLength(const AppState *app)
{
    switch (app->view) {
    case APP_VIEW_CALCULATOR:
        return app->expressionLength;
    case APP_VIEW_MODE_FORM:
        return app->formLengths[app->formField];
    case APP_VIEW_MODE_GRID:
        return app->gridCellLengths[app->gridPanel][app->gridRow]
                                   [app->gridColumn];
    case APP_VIEW_STAT_DATA:
        return app->statCellLengths[app->statRow][app->statColumn];
    default:
        return 0;
    }
}

static void checkSharedKeypadEdges(AppState *app, u8 pageCount)
{
    app->tokenPage = 0;
    app->selectedToken = 12;
    pressKey(app, APP_KEY_LEFT);
    check(app->tokenPage == pageCount - 1 && app->selectedToken == 17,
          "LEFT across column zero wraps to the preceding page");

    app->tokenPage = (u8)(pageCount - 1);
    app->selectedToken = 17;
    pressKey(app, APP_KEY_RIGHT);
    check(app->tokenPage == 0 && app->selectedToken == 12,
          "RIGHT across column five wraps to the following page");

    app->tokenPage = 0;
    app->selectedToken = 3;
    pressKey(app, APP_KEY_UP);
    check(app->tokenPage == pageCount - 1 && app->selectedToken == 27,
          "UP across row zero wraps to the preceding page");

    app->tokenPage = (u8)(pageCount - 1);
    app->selectedToken = 27;
    pressKey(app, APP_KEY_DOWN);
    check(app->tokenPage == 0 && app->selectedToken == 3,
          "DOWN across row four wraps to the following page");
}

static void testSharedWorkspaceKeypads(void)
{
    static const CalcMode modes[] = {
        CALC_MODE_COMP, CALC_MODE_CMPLX, CALC_MODE_STAT,
        CALC_MODE_EQN, CALC_MODE_MATRIX, CALC_MODE_TABLE,
        CALC_MODE_VECTOR, CALC_MODE_INEQ, CALC_MODE_RATIO,
        CALC_MODE_DIST
    };
    AppState app;
    u8 modeIndex;

    for (modeIndex = 0; modeIndex < GCALC_ARRAY_COUNT(modes);
         modeIndex++) {
        CalcMode mode = modes[modeIndex];
        AppView view = beginDataWorkspace(&app, mode, 0);
        u8 pageCount = appModeKeypadPageCount(mode);
        u8 page;
        u8 index;

        check(view == APP_VIEW_CALCULATOR || view == APP_VIEW_MODE_FORM ||
              view == APP_VIEW_MODE_GRID || view == APP_VIEW_STAT_DATA,
              "mode exposes a keypad-backed owned workspace");
        check(pageCount != 0, "keypad-backed mode owns at least one page");
        checkSharedKeypadEdges(&app, pageCount);

        for (page = 0; page < pageCount; page++) {
            app.tokenPage = page;
            app.selectedToken = 0;
            app.dirty = APP_DIRTY_ALL;
            render(&app);
            check(app.dirty == APP_DIRTY_NONE,
                  "every owned keypad page renders all 6x5 cells");

            for (index = 0; index < KEYPAD_EXE; index++) {
                clearCurrentEditor(&app);
                app.selectedToken = index;
                pressKey(&app, APP_KEY_A);
                check(currentEditorLength(&app) != 0,
                      "every non-EXE shared keypad slot is initialized");
            }

            app.selectedToken = KEYPAD_EXE;
            if (view == APP_VIEW_CALCULATOR) {
                appSetExpression(&app, "1+1");
                pressKey(&app, APP_KEY_A);
                check(app.runtime.error == CALC_OK &&
                      strcmp(app.runtime.result, "2") == 0,
                      "slot 29 is EXE on every calculator page");
            } else if (view == APP_VIEW_MODE_FORM) {
                memset(app.formFields, 0, sizeof(app.formFields));
                memset(app.formLengths, 0, sizeof(app.formLengths));
                naturalCursorSetEnd(&app.formCursor, 0);
                pressKey(&app, APP_KEY_A);
                check(app.formLengths[0] == 0 &&
                      strstr(app.status, "COMPLETE") != 0,
                      "slot 29 executes rather than inserting in forms");
            } else if (view == APP_VIEW_MODE_GRID) {
                u8 oldPanel = app.gridPanel;
                u8 oldRow = app.gridRow;
                u8 oldColumn = app.gridColumn;
                clearCurrentEditor(&app);
                pressKey(&app, APP_KEY_A);
                check(app.gridPanel != oldPanel || app.gridRow != oldRow ||
                      app.gridColumn != oldColumn,
                      "slot 29 advances the active grid cell");
            } else {
                u8 oldRow = app.statRow;
                u8 oldColumn = app.statColumn;
                clearCurrentEditor(&app);
                pressKey(&app, APP_KEY_A);
                check(app.statRow != oldRow || app.statColumn != oldColumn,
                      "slot 29 advances the active STAT cell");
            }
        }
    }
}

static void setFormField(AppState *app, u8 field, const char *text)
{
    copyText(app->formFields[field], &app->formLengths[field],
             APP_FORM_FIELD_CAPACITY + 1, text);
}

static void testFormFlows(void)
{
    AppState app;

    check(beginDataWorkspace(&app, CALC_MODE_TABLE, 0) ==
          APP_VIEW_MODE_FORM, "TABLE action opens its dedicated form");
    setFormField(&app, 0, "x^2");
    setFormField(&app, 1, "0");
    setFormField(&app, 2, "2");
    setFormField(&app, 3, "1");
    pressKey(&app, APP_KEY_START);
    check(app.runtime.error == CALC_OK && app.view == APP_VIEW_TABLE &&
          app.runtime.table.rows == 3,
          "TABLE form evaluates and opens the table result");
    pressKey(&app, APP_KEY_B);
    check(app.view == APP_VIEW_MODE_FORM &&
          strcmp(app.formFields[0], "x^2") == 0,
          "B from TABLE result restores its populated form");

    check(beginDataWorkspace(&app, CALC_MODE_RATIO, 0) ==
          APP_VIEW_MODE_FORM, "RATIO action opens its dedicated form");
    setFormField(&app, 0, "1");
    setFormField(&app, 1, "2");
    setFormField(&app, 2, "6");
    pressKey(&app, APP_KEY_START);
    check(app.runtime.error == CALC_OK &&
          strcmp(app.runtime.result, "3") == 0 &&
          strcmp(app.expression, "ratio1(1;2;6)") == 0,
          "RATIO form serializes its fields in the owned command order");

    check(beginDataWorkspace(&app, CALC_MODE_DIST, 4) ==
          APP_VIEW_MODE_FORM, "DIST action opens its dedicated form");
    setFormField(&app, 0, "0");
    setFormField(&app, 1, "1");
    setFormField(&app, 2, "0");
    pressKey(&app, APP_KEY_START);
    check(app.runtime.error == CALC_OK &&
          strstr(app.runtime.result, "0.398") != 0 &&
          strcmp(app.expression, "normpdf(0;1;0)") == 0,
          "DIST normal form uses canonical x;sigma;mu order");

    enterMode(&app, CALC_MODE_RATIO);
    pressKey(&app, APP_KEY_A);
    check(app.view == APP_VIEW_MODE_FORM, "RATIO form reopens");
    pressKey(&app, APP_KEY_B);
    check(app.view == APP_VIEW_MODE_ACTION,
          "B on an empty form returns to the action selector");
}

static void setGridCell(AppState *app, u8 panel, u8 row, u8 column,
                        const char *text)
{
    copyText(app->gridCells[panel][row][column],
             &app->gridCellLengths[panel][row][column],
             APP_GRID_CELL_CAPACITY + 1, text);
}

static void testSimpleGridFlows(void)
{
    static const char *const equation[2][3] = {
        {"1", "1", "3"}, {"2", "-1", "0"}
    };
    AppState app;
    u8 row;
    u8 column;

    check(beginDataWorkspace(&app, CALC_MODE_EQN, 0) ==
          APP_VIEW_MODE_GRID && app.gridRows[0] == 2 &&
          app.gridColumns[0] == 3,
          "EQN opens a 2x3 coefficient grid for two unknowns");
    for (row = 0; row < 2; row++)
        for (column = 0; column < 3; column++)
            setGridCell(&app, 0, row, column, equation[row][column]);
    pressKey(&app, APP_KEY_START);
    check(app.runtime.error == CALC_OK &&
          strstr(app.runtime.result, "X=1") != 0 &&
          strstr(app.runtime.result, "Y=2") != 0,
          "EQN coefficient grid evaluates its linear system");
}

static void testMatrixRectangularGrid(void)
{
    static const char *const left[2][3] = {
        {"1", "2", "3"}, {"4", "5", "6"}
    };
    static const char *const right[3][2] = {
        {"7", "8"}, {"9", "10"}, {"11", "12"}
    };
    AppState app;
    u8 row;
    u8 column;

    check(beginDataWorkspace(&app, CALC_MODE_MATRIX, 0) ==
          APP_VIEW_MODE_GRID && app.gridRows[0] == 4 &&
          app.gridColumns[0] == 4,
          "EDIT MatA opens its default 4x4 register table");
    /* 4 -> 1 -> 2 rows, and 4 -> 1 -> 2 -> 3 columns. */
    pressKey(&app, APP_KEY_SELECT | APP_KEY_R);
    pressKey(&app, APP_KEY_SELECT | APP_KEY_R);
    pressKey(&app, APP_KEY_SELECT | APP_KEY_L);
    pressKey(&app, APP_KEY_SELECT | APP_KEY_L);
    pressKey(&app, APP_KEY_SELECT | APP_KEY_L);
    check(app.gridRows[0] == 2 && app.gridColumns[0] == 3,
          "MatA register table accepts a rectangular 2x3 shape");
    for (row = 0; row < 2; row++)
        for (column = 0; column < 3; column++)
            setGridCell(&app, 0, row, column, left[row][column]);
    pressKey(&app, APP_KEY_START);
    check(app.view == APP_VIEW_MODE_ACTION,
          "START stores MatA and exits its register table");

    app.actionSelection = 1;
    pressKey(&app, APP_KEY_A);
    check(app.view == APP_VIEW_MODE_GRID && app.gridRows[0] == 4 &&
          app.gridColumns[0] == 4,
          "EDIT MatB opens an independent default 4x4 table");
    pressKey(&app, APP_KEY_SELECT | APP_KEY_R);
    pressKey(&app, APP_KEY_SELECT | APP_KEY_R);
    pressKey(&app, APP_KEY_SELECT | APP_KEY_R);
    pressKey(&app, APP_KEY_SELECT | APP_KEY_L);
    pressKey(&app, APP_KEY_SELECT | APP_KEY_L);
    check(app.gridRows[0] == 3 && app.gridColumns[0] == 2,
          "MatB register table accepts a rectangular 3x2 shape");
    for (row = 0; row < 3; row++)
        for (column = 0; column < 2; column++)
            setGridCell(&app, 0, row, column, right[row][column]);
    pressKey(&app, APP_KEY_START);

    app.actionSelection = 9;
    pressKey(&app, APP_KEY_A);
    check(app.view == APP_VIEW_MODE_FORM &&
          strcmp(app.formFields[0], "MatA") == 0 &&
          strcmp(app.formFields[1], "MatB") == 0,
          "mul() opens a canonical MatA/MatB operation form");
    pressKey(&app, APP_KEY_START);
    check(app.runtime.error == CALC_OK &&
          strcmp(app.expression, "mul(MatA;MatB)") == 0 &&
          strstr(app.runtime.result, "58,64") != 0 &&
          strstr(app.runtime.result, "139,154") != 0,
          "rectangular named MATRIX registers multiply correctly");
}

static void testInequalityDegreeGrids(void)
{
    static const struct InequalityCase {
        u8 action;
        u8 columns;
        const char *coefficient[5];
        const char *needle;
    } cases[] = {
        {0, 3, {"1", "0", "-4", 0, 0}, "-2<X<2"},
        {1, 4, {"1", "0", "0", "0", 0}, "X<0"},
        {2, 5, {"1", "0", "0", "0", "-1"}, "-1<X<1"}
    };
    AppState app;
    u8 caseIndex;

    for (caseIndex = 0; caseIndex < GCALC_ARRAY_COUNT(cases);
         caseIndex++) {
        u8 column;
        check(beginDataWorkspace(&app, CALC_MODE_INEQ,
                                 cases[caseIndex].action) ==
              APP_VIEW_MODE_GRID &&
              app.gridColumns[0] == cases[caseIndex].columns,
              "INEQ degree selector opens the matching coefficient grid");
        for (column = 0; column < cases[caseIndex].columns; column++)
            setGridCell(&app, 0, 0, column,
                        cases[caseIndex].coefficient[column]);
        pressKey(&app, APP_KEY_START);
        check(app.runtime.error == CALC_OK &&
              strstr(app.runtime.result, cases[caseIndex].needle) != 0,
              "INEQ degree 2/3/4 grid evaluates its coefficients");
    }
}

static void setStatCell(AppState *app, u8 row, u8 column,
                        const char *text)
{
    if (text == 0)
        return;
    copyText(app->statCells[row][column],
             &app->statCellLengths[row][column],
             APP_STAT_CELL_CAPACITY + 1, text);
}

static void testStatUi(void)
{
    static const struct StatCase {
        StatModel model;
        u8 rows;
        const char *x[4];
        const char *y[4];
        const char *frequency[4];
        const char *needleA;
        const char *needleB;
    } cases[] = {
        {STAT_MODEL_1VAR, 3, {"1", "2", "3", 0},
         {0, 0, 0, 0}, {"2", "1", "1", 0}, "N=4", "M=1.75"},
        {STAT_MODEL_LINEAR, 4, {"1", "2", "3", "4"},
         {"5", "8", "11", "14"}, {0, 0, 0, 0}, "A=2", "B=3"},
        {STAT_MODEL_QUADRATIC, 4, {"0", "1", "2", "3"},
         {"1", "6", "17", "34"}, {0, 0, 0, 0}, "A=1", "C=3"},
        {STAT_MODEL_LOGARITHMIC, 3,
         {"1", "2.718281828", "7.389056099", 0},
         {"2", "5", "8", 0}, {0, 0, 0, 0}, "A=2", "B=3"},
        {STAT_MODEL_EXPONENTIAL, 3, {"0", "1", "2", 0},
         {"2", "5.436563657", "14.7781122", 0},
         {0, 0, 0, 0}, "A=2", "B=1"},
        {STAT_MODEL_AB_EXPONENTIAL, 3, {"0", "1", "2", 0},
         {"2", "6", "18", 0}, {0, 0, 0, 0}, "A=2", "B=3"},
        {STAT_MODEL_POWER, 3, {"1", "2", "4", 0},
         {"2", "8", "32", 0}, {0, 0, 0, 0}, "A=2", "B=2"},
        {STAT_MODEL_INVERSE, 3, {"1", "2", "4", 0},
         {"5", "3", "2", 0}, {0, 0, 0, 0}, "A=1", "B=4"}
    };
    AppState app;
    u8 caseIndex;

    for (caseIndex = 0; caseIndex < GCALC_ARRAY_COUNT(cases);
         caseIndex++) {
        const struct StatCase *test = &cases[caseIndex];
        u8 row;
        enterMode(&app, CALC_MODE_STAT);
        app.actionSelection = (u8)test->model;
        pressKey(&app, APP_KEY_A);
        check(app.view == APP_VIEW_STAT_DATA &&
              modeGetStatModel(&app.runtime) == test->model,
              "all eight STAT selector items open their data view");
        for (row = 0; row < test->rows; row++) {
            setStatCell(&app, row, 0, test->x[row]);
            if (test->model == STAT_MODEL_1VAR)
                setStatCell(&app, row, 1, test->frequency[row]);
            else {
                setStatCell(&app, row, 1, test->y[row]);
                setStatCell(&app, row, 2, test->frequency[row]);
            }
        }
        pressKey(&app, APP_KEY_START);
        check(app.runtime.error == CALC_OK &&
              strstr(app.runtime.result, test->needleA) != 0 &&
              strstr(app.runtime.result, test->needleB) != 0,
              "all eight STAT data tables produce their result");
        app.dirty = APP_DIRTY_ALL;
        render(&app);
        check(app.dirty == APP_DIRTY_NONE,
              "STAT data and result view renders completely");

        app.statRow = APP_STAT_MAX_ROWS - 1;
        app.statColumn = 0;
        app.statCells[app.statRow][0][0] = '\0';
        app.statCellLengths[app.statRow][0] = 0;
        naturalCursorSetEnd(&app.formCursor, 0);
        pressKey(&app, APP_KEY_B);
        check(app.view == APP_VIEW_STAT_TYPE &&
              app.actionSelection == (u8)test->model,
              "B on an empty STAT cell restores the eight-type selector");
    }
}

static void setBaseExpression(AppState *app, const char *text)
{
    copyText(app->baseExpression, &app->baseExpressionLength,
             APP_EXPRESSION_CAPACITY + 1, text);
    naturalCursorSetEnd(&app->baseCursor, app->baseExpressionLength);
}

static void testBaseUi(void)
{
    static const BaseRadix radixForCell[4] = {
        BASE_RADIX_DEC, BASE_RADIX_HEX, BASE_RADIX_BIN, BASE_RADIX_OCT
    };
    AppState app;
    u8 page;
    u8 index;

    enterMode(&app, CALC_MODE_BASEN);
    check(app.view == APP_VIEW_BASEN &&
          appModeKeypadPageCount(CALC_MODE_BASEN) == 2,
          "BASE-N owns a dedicated two-page editor");
    checkSharedKeypadEdges(&app, 2);

    for (page = 0; page < 2; page++) {
        app.tokenPage = page;
        app.dirty = APP_DIRTY_ALL;
        render(&app);
        check(app.dirty == APP_DIRTY_NONE,
              "both BASE-N 6x5 keypad pages render");
        for (index = 0; index < KEYPAD_EXE; index++) {
            setBaseExpression(&app, "");
            modeSetBaseRadix(&app.runtime, BASE_RADIX_HEX);
            app.selectedToken = index;
            pressKey(&app, APP_KEY_A);
            if (index < 4)
                check(modeGetBaseRadix(&app.runtime) ==
                      radixForCell[index],
                      "BASE-N radix selector occupies cells 0..3");
            else
                check(app.baseExpressionLength != 0,
                      "every enabled non-EXE BASE-N slot is initialized");
        }
        modeSetBaseRadix(&app.runtime, BASE_RADIX_HEX);
        setBaseExpression(&app, "F");
        app.selectedToken = KEYPAD_EXE;
        pressKey(&app, APP_KEY_A);
        check(app.runtime.error == CALC_OK &&
              strstr(app.runtime.result, "DEC=15") != 0,
              "slot 29 is EXE on both BASE-N pages");
    }

    app.tokenPage = 0;
    app.selectedToken = 2;
    pressKey(&app, APP_KEY_A);
    setBaseExpression(&app, "");
    app.selectedToken = 25;
    pressKey(&app, APP_KEY_A);
    check(app.baseExpressionLength == 0 &&
          strstr(app.status, "DIGIT NOT IN RADIX") != 0,
          "BIN disables digit 2 at the keypad boundary");
    app.selectedToken = 24;
    pressKey(&app, APP_KEY_A);
    check(strcmp(app.baseExpression, "1") == 0,
          "BIN keeps digit 1 enabled");

    modeSetBaseRadix(&app.runtime, BASE_RADIX_HEX);
    setBaseExpression(&app, "neg(1)");
    app.selectedToken = KEYPAD_EXE;
    pressKey(&app, APP_KEY_A);
    check(app.runtime.error == CALC_OK &&
          strstr(app.runtime.result, "DEC=-1") != 0 &&
          strstr(app.runtime.result, "HEX=FFFFFFFF") != 0,
          "BASE-N Neg uses the full 32-bit two's-complement value");
    setBaseExpression(&app, "xnor(F;7)");
    pressKey(&app, APP_KEY_A);
    check(app.runtime.error == CALC_OK &&
          strstr(app.runtime.result, "DEC=-9") != 0,
          "BASE-N XNOR complements all 32 bits through the UI");

    app.tokenPage = 0;
    app.selectedToken = 2;
    pressKey(&app, APP_KEY_A);
    setBaseExpression(&app, "101+1");
    app.selectedToken = KEYPAD_EXE;
    pressKey(&app, APP_KEY_A);
    check(app.runtime.error == CALC_OK &&
          strstr(app.runtime.result, "BIN=110") != 0 &&
          strstr(app.runtime.result, "DEC=6") != 0,
          "BASE-N UI evaluates arithmetic in the selected BIN radix");
    app.selectedToken = 1;
    pressKey(&app, APP_KEY_A);
    setBaseExpression(&app, "1F+1");
    app.selectedToken = KEYPAD_EXE;
    pressKey(&app, APP_KEY_A);
    check(app.runtime.error == CALC_OK &&
          strstr(app.runtime.result, "HEX=20") != 0,
          "BASE-N UI evaluates arithmetic in the selected HEX radix");
}

int main(void)
{
    initializeFramebuffer();
    testSharedWorkspaceKeypads();
    testFormFlows();
    testSimpleGridFlows();
    testMatrixRectangularGrid();
    testInequalityDegreeGrids();
    testStatUi();
    testBaseUi();
    checkFramebufferGuards();

    if (failures != 0) {
        fprintf(stderr, "app_workspace_test: %d failure(s)\n", failures);
        return 1;
    }
    puts("app_workspace_test: PASS");
    return 0;
}
