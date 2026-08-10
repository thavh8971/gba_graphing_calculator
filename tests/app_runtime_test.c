#include <stdio.h>
#include <string.h>

#include "gcalc/app.h"

#define FRAMEBUFFER_GUARD_WORDS 32
#define FRAMEBUFFER_SENTINEL 0x5aa5
#define FRAMEBUFFER_WORDS (GBA_SCREEN_WIDTH * GBA_SCREEN_HEIGHT)

static u16 framebufferStorage[FRAMEBUFFER_GUARD_WORDS + FRAMEBUFFER_WORDS +
                              FRAMEBUFFER_GUARD_WORDS];
#define framebuffer (framebufferStorage + FRAMEBUFFER_GUARD_WORDS)
static int failures;

#define SHARED_KEYPAD_COLUMNS 6
#define SHARED_KEYPAD_ROWS 5
#define SHARED_KEYPAD_EXE_INDEX 29
#define GRAPH_KEYPAD_COLUMNS 6
#define GRAPH_KEYPAD_ROWS 5
#define GRAPH_KEYPAD_EXE_INDEX 29
#define GRAPH_KEYPAD_PAGES 4

static void check(int condition, const char *message)
{
    if (!condition) {
        printf("FAIL app runtime: %s\n", message);
        failures++;
    }
}

static void initializeFramebufferGuards(void)
{
    u16 index;

    for (index = 0; index < FRAMEBUFFER_GUARD_WORDS; index++) {
        framebufferStorage[index] = FRAMEBUFFER_SENTINEL;
        framebufferStorage[FRAMEBUFFER_GUARD_WORDS + FRAMEBUFFER_WORDS +
                           index] = FRAMEBUFFER_SENTINEL;
    }
}

static void checkFramebufferGuards(void)
{
    u16 index;
    int intact = 1;

    for (index = 0; index < FRAMEBUFFER_GUARD_WORDS; index++) {
        if (framebufferStorage[index] != FRAMEBUFFER_SENTINEL ||
            framebufferStorage[FRAMEBUFFER_GUARD_WORDS + FRAMEBUFFER_WORDS +
                               index] != FRAMEBUFFER_SENTINEL) {
            intact = 0;
            break;
        }
    }
    check(intact, "app rendering stays inside the Mode 3 framebuffer");
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

static void pressKey(AppState *app, u16 key)
{
    appHandleKeys(app, key, key);
}

static void checkSelectedExeGlyphHeight(s16 keypadY, s16 keypadHeight,
                                        const char *message)
{
    const u16 textColor = RGB15(1, 2, 4);
    const s16 column = SHARED_KEYPAD_COLUMNS - 1;
    const s16 row = SHARED_KEYPAD_ROWS - 1;
    const s16 cellWidth = GBA_SCREEN_WIDTH / SHARED_KEYPAD_COLUMNS;
    const s16 x = (s16)(column * cellWidth);
    const s16 y = (s16)(keypadY +
                        (row * keypadHeight) / SHARED_KEYPAD_ROWS);
    const s16 bottom = (s16)(keypadY +
                             ((row + 1) * keypadHeight) /
                             SHARED_KEYPAD_ROWS);
    s16 first = -1;
    s16 last = -1;
    s16 pixelX;
    s16 pixelY;

    for (pixelY = (s16)(y + 1); pixelY < bottom - 1; pixelY++) {
        for (pixelX = (s16)(x + 1); pixelX < x + cellWidth - 1;
             pixelX++) {
            if (framebuffer[pixelY * GBA_SCREEN_WIDTH + pixelX] ==
                textColor) {
                if (first < 0)
                    first = pixelY;
                last = pixelY;
            }
        }
    }
    check(first >= 0 && last - first + 1 == GFX_ASCII5X7_HEIGHT,
          message);
}

static void testSharedEditorKeypad(void)
{
    AppState app;
    u8 pageCount = appModeKeypadPageCount(CALC_MODE_COMP);
    u8 index;
    u8 page;
    u8 step;

    appInit(&app);
    app.mode = CALC_MODE_COMP;
    app.view = APP_VIEW_CALCULATOR;

    app.tokenPage = 0;
    app.selectedToken = 0;
    for (step = 1; step < SHARED_KEYPAD_COLUMNS; step++)
        pressKey(&app, APP_KEY_RIGHT);
    for (step = 1; step < SHARED_KEYPAD_ROWS; step++)
        pressKey(&app, APP_KEY_DOWN);
    check(app.tokenPage == 0 &&
          app.selectedToken == SHARED_KEYPAD_EXE_INDEX,
          "shared keypad navigation exposes a 6x5 bottom-right cell");

    app.dirty = APP_DIRTY_NONE;
    app.tokenPage = 0;
    app.selectedToken = 12;
    pressKey(&app, APP_KEY_LEFT);
    check(app.tokenPage == pageCount - 1 &&
          app.selectedToken == 17,
          "shared LEFT beyond column zero changes page and preserves row");
    check((app.dirty & (APP_DIRTY_HEADER | APP_DIRTY_STATUS |
                        APP_DIRTY_KEYPAD)) ==
          (APP_DIRTY_HEADER | APP_DIRTY_STATUS | APP_DIRTY_KEYPAD),
          "shared page change redraws header, status, and keypad");

    app.tokenPage = (u8)(pageCount - 1);
    app.selectedToken = 17;
    pressKey(&app, APP_KEY_RIGHT);
    check(app.tokenPage == 0 && app.selectedToken == 12,
          "shared RIGHT beyond column five changes page and preserves row");

    app.tokenPage = 0;
    app.selectedToken = 3;
    pressKey(&app, APP_KEY_UP);
    check(app.tokenPage == pageCount - 1 &&
          app.selectedToken == 27,
          "shared UP beyond row zero changes page and preserves column");

    app.tokenPage = (u8)(pageCount - 1);
    app.selectedToken = 27;
    pressKey(&app, APP_KEY_DOWN);
    check(app.tokenPage == 0 && app.selectedToken == 3,
          "shared DOWN beyond row four changes page and preserves column");

    for (page = 0; page < pageCount; page++) {
        for (index = 0; index <= SHARED_KEYPAD_EXE_INDEX; index++) {
            app.mode = CALC_MODE_COMP;
            app.view = APP_VIEW_CALCULATOR;
            app.tokenPage = page;
            app.selectedToken = index;
            app.menuSelection = 0xff;
            app.dirty = APP_DIRTY_ALL;
            render(&app);
            check(app.dirty == APP_DIRTY_NONE,
                  "every shared keypad slot renders on every page");

            if (index < SHARED_KEYPAD_EXE_INDEX) {
                appSetExpression(&app, "");
                pressKey(&app, APP_KEY_A);
                check(app.expressionLength != 0,
                      "every non-EXE shared keypad slot is initialized");
            } else {
                appSetExpression(&app, "1+1");
                pressKey(&app, APP_KEY_A);
                check(app.runtime.error == CALC_OK &&
                      strcmp(app.runtime.result, "2") == 0 &&
                      strcmp(app.expression, "1+1") == 0,
                      "bottom-right EXE evaluates from every shared page");
                if (page == 0) {
                    app.dirty = APP_DIRTY_ALL;
                    render(&app);
                    checkSelectedExeGlyphHeight(
                        82, 78,
                        "shared editor keypad labels render at 5x7 height");
                }
            }
        }
    }
}

static void testModeEntryContracts(void)
{
    static const struct ModeCase {
        CalcMode mode;
        AppView entryView;
        u8 keypadPages;
    } cases[] = {
        {CALC_MODE_COMP, APP_VIEW_CALCULATOR, 7},
        {CALC_MODE_CMPLX, APP_VIEW_CALCULATOR, 8},
        {CALC_MODE_STAT, APP_VIEW_STAT_TYPE, 3},
        {CALC_MODE_BASEN, APP_VIEW_BASEN, 2},
        {CALC_MODE_EQN, APP_VIEW_MODE_ACTION, 4},
        {CALC_MODE_MATRIX, APP_VIEW_MODE_ACTION, 3},
        {CALC_MODE_TABLE, APP_VIEW_MODE_ACTION, 5},
        {CALC_MODE_VECTOR, APP_VIEW_MODE_ACTION, 3},
        {CALC_MODE_INEQ, APP_VIEW_MODE_ACTION, 4},
        {CALC_MODE_RATIO, APP_VIEW_MODE_ACTION, 2},
        {CALC_MODE_DIST, APP_VIEW_MODE_ACTION, 3},
        {CALC_MODE_GRAPHING, APP_VIEW_GRAPH_INPUT, 4}
    };
    AppState app;
    u8 index;

    for (index = 0; index < GCALC_ARRAY_COUNT(cases); index++) {
        appInit(&app);
        appSetExpression(&app, "2+2");
        appSetGraphExpression(&app, "y=x");
        strcpy(app.baseExpression, "FF");
        app.baseExpressionLength = 2;
        appHandleKeys(&app, APP_KEY_SELECT | APP_KEY_START, 0);
        app.menuSelection = (u8)cases[index].mode;
        pressKey(&app, APP_KEY_A);
        check(app.mode == cases[index].mode &&
              app.view == cases[index].entryView &&
              appModeEntryView(cases[index].mode) == cases[index].entryView,
              "each mode enters its owned workspace");
        check(appModeKeypadPageCount(cases[index].mode) ==
              cases[index].keypadPages,
              "each mode exposes only its owned keypad pages");
        check(strcmp(app.expression, "2+2") == 0 &&
              strcmp(app.graphExpression, "y=x") == 0 &&
              strcmp(app.baseExpression, "FF") == 0,
              "changing mode preserves isolated editor buffers");
        render(&app);
        check(app.dirty == APP_DIRTY_NONE,
              "every mode entry workspace renders completely");
    }
}

static void testSharedEditorSelectionClamps(void)
{
    AppState app;

    appInit(&app);
    app.mode = CALC_MODE_TABLE;
    app.view = APP_VIEW_CALCULATOR;
    app.selectedToken = SHARED_KEYPAD_EXE_INDEX;
    appSetExpression(&app, "x^2;0;2;1");
    pressKey(&app, APP_KEY_A);
    check(app.view == APP_VIEW_TABLE,
          "TABLE shared EXE opens the table view");
    app.selectedToken = 200;
    pressKey(&app, APP_KEY_B);
    check(app.view == APP_VIEW_CALCULATOR &&
          app.selectedToken <= SHARED_KEYPAD_EXE_INDEX,
          "TABLE return clamps an out-of-range shared selection");
    render(&app);

    app.selectedToken = 200;
    appHandleKeys(&app, APP_KEY_SELECT | APP_KEY_START, 0);
    check(app.view == APP_VIEW_MENU,
          "menu opens with stale shared selection state");
    render(&app);
    pressKey(&app, APP_KEY_B);
    check(app.view == APP_VIEW_CALCULATOR &&
          app.selectedToken <= SHARED_KEYPAD_EXE_INDEX,
          "menu cancel clamps an out-of-range shared selection");
    render(&app);
}

static void testGraphInputKeypad(void)
{
    static const char graphSource[] = "y=x^2";
    AppState app;
    u8 index;
    u8 page;
    u8 step;

    appInit(&app);
    appSetExpression(&app, "2+2");
    appSetGraphExpression(&app, graphSource);
    appHandleKeys(&app, APP_KEY_SELECT | APP_KEY_START, 0);
    app.menuSelection = CALC_MODE_GRAPHING;
    pressKey(&app, APP_KEY_A);
    check(app.mode == CALC_MODE_GRAPHING,
          "GRAPH menu selection changes mode");
    check(app.view == APP_VIEW_GRAPH_INPUT,
          "GRAPH menu opens dedicated expression entry");
    check(strcmp(app.expression, "2+2") == 0 &&
          strcmp(app.graphExpression, graphSource) == 0,
          "GRAPH entry owns an expression separate from COMP");

    render(&app);
    check(app.dirty == APP_DIRTY_NONE,
          "GRAPH expression entry render consumes dirty regions");

    app.graphTokenPage = 0;
    app.graphSelectedToken = 0;
    for (step = 1; step < GRAPH_KEYPAD_COLUMNS; step++)
        pressKey(&app, APP_KEY_RIGHT);
    for (step = 1; step < GRAPH_KEYPAD_ROWS; step++)
        pressKey(&app, APP_KEY_DOWN);
    check(app.graphTokenPage == 0 &&
          app.graphSelectedToken == GRAPH_KEYPAD_EXE_INDEX,
          "GRAPH keypad navigation exposes a 6x5 bottom-right cell");

    app.graphTokenPage = 0;
    app.graphSelectedToken = 12;
    pressKey(&app, APP_KEY_LEFT);
    check(app.graphTokenPage == GRAPH_KEYPAD_PAGES - 1 &&
          app.graphSelectedToken == 17,
          "GRAPH LEFT beyond column zero changes page and preserves row");
    check((app.dirty & (APP_DIRTY_HEADER | APP_DIRTY_STATUS |
                        APP_DIRTY_KEYPAD)) ==
          (APP_DIRTY_HEADER | APP_DIRTY_STATUS | APP_DIRTY_KEYPAD),
          "GRAPH page change redraws header, status, and keypad");

    app.graphTokenPage = GRAPH_KEYPAD_PAGES - 1;
    app.graphSelectedToken = 17;
    pressKey(&app, APP_KEY_RIGHT);
    check(app.graphTokenPage == 0 && app.graphSelectedToken == 12,
          "GRAPH RIGHT beyond column five changes page and preserves row");

    app.graphTokenPage = 0;
    app.graphSelectedToken = 3;
    pressKey(&app, APP_KEY_UP);
    check(app.graphTokenPage == GRAPH_KEYPAD_PAGES - 1 &&
          app.graphSelectedToken == 27,
          "GRAPH UP beyond row zero changes page and preserves column");

    app.graphTokenPage = GRAPH_KEYPAD_PAGES - 1;
    app.graphSelectedToken = 27;
    pressKey(&app, APP_KEY_DOWN);
    check(app.graphTokenPage == 0 && app.graphSelectedToken == 3,
          "GRAPH DOWN beyond row four changes page and preserves column");
    check(strcmp(app.graphExpression, graphSource) == 0,
          "GRAPH page navigation does not edit the expression");

    for (page = 0; page < GRAPH_KEYPAD_PAGES; page++) {
        for (index = 0; index < GRAPH_KEYPAD_EXE_INDEX; index++) {
            app.mode = CALC_MODE_GRAPHING;
            app.view = APP_VIEW_GRAPH_INPUT;
            app.graphTokenPage = page;
            app.graphSelectedToken = index;
            appSetGraphExpression(&app, "");
            pressKey(&app, APP_KEY_A);
            check(app.graphExpressionLength != 0,
                  "every non-EXE GRAPH keypad slot is initialized");
        }
        app.mode = CALC_MODE_GRAPHING;
        app.view = APP_VIEW_GRAPH_INPUT;
        app.graphTokenPage = page;
        app.graphSelectedToken = GRAPH_KEYPAD_EXE_INDEX;
        appSetGraphExpression(&app, graphSource);
        if (page == 0) {
            app.dirty = APP_DIRTY_ALL;
            render(&app);
            checkSelectedExeGlyphHeight(
                60, 100,
                "GRAPH input keypad labels render at 5x7 height");
        }
        pressKey(&app, APP_KEY_A);
        check(app.view == APP_VIEW_GRAPH &&
              app.runtime.error == CALC_OK &&
              app.graphFunctionCount == 1,
              "bottom-right EXE+A plots from every GRAPH keypad page");
        if (app.graphFunctionCount != 0)
            check(strcmp(app.graphFunctions[0].source, graphSource) == 0,
                  "GRAPH EXE parses the current GRAPH expression");
        pressKey(&app, APP_KEY_B);
        check(app.view == APP_VIEW_GRAPH_INPUT &&
              app.mode == CALC_MODE_GRAPHING,
              "B from graph returns to GRAPH expression entry");
        check(strcmp(app.graphExpression, graphSource) == 0,
              "B from graph preserves the GRAPH expression");
    }
}

static void testMenuReturnViews(void)
{
    static const AppView views[] = {
        APP_VIEW_CALCULATOR,
        APP_VIEW_TABLE,
        APP_VIEW_GRAPH_INPUT,
        APP_VIEW_GRAPH
    };
    AppState app;
    u8 index;
    u8 expectedSelection;

    for (index = 0; index < GCALC_ARRAY_COUNT(views); index++) {
        appInit(&app);
        app.view = views[index];
        app.mode = views[index] == APP_VIEW_CALCULATOR ?
                   CALC_MODE_COMP :
                   (views[index] == APP_VIEW_TABLE ? CALC_MODE_TABLE :
                                                    CALC_MODE_GRAPHING);
        if (views[index] == APP_VIEW_CALCULATOR) {
            app.selectedToken = 23;
            expectedSelection = 23;
        } else if (views[index] == APP_VIEW_TABLE) {
            app.runtime.table.rows = TABLE_MAX_ROWS;
            app.selectedToken = 2;
            expectedSelection = 2;
        } else if (views[index] == APP_VIEW_GRAPH) {
            app.graphFunctionCount = 2;
            app.selectedToken = 1;
            expectedSelection = 1;
        } else {
            app.graphSelectedToken = 23;
            expectedSelection = app.selectedToken;
        }
        appHandleKeys(&app, APP_KEY_SELECT | APP_KEY_START, 0);
        check(app.view == APP_VIEW_MENU &&
              app.menuReturnView == views[index],
              "mode menu records the originating view");
        pressKey(&app, APP_KEY_RIGHT);
        pressKey(&app, APP_KEY_B);
        check(app.view == views[index] &&
              app.selectedToken == expectedSelection,
              "B restores the originating view and owned selection");
        if (views[index] == APP_VIEW_GRAPH_INPUT)
            check(app.graphSelectedToken == 23,
                  "menu cancel preserves GRAPH input keypad selection");

        appHandleKeys(&app, APP_KEY_SELECT | APP_KEY_START, 0);
        check(app.view == APP_VIEW_MENU &&
              app.menuReturnView == views[index],
              "mode menu can reopen from every interactive view");
        pressKey(&app, APP_KEY_DOWN);
        appHandleKeys(&app, APP_KEY_SELECT | APP_KEY_START, 0);
        check(app.view == views[index] &&
              app.selectedToken == expectedSelection,
              "SELECT+START restores the originating view and selection");
    }
}

int main(void)
{
    AppState app;
    s16 oldCamera;
    u8 oldZoom;

    initializeFramebufferGuards();
    appInit(&app);
    check(app.mode == CALC_MODE_COMP, "initial mode");
    check(app.view == APP_VIEW_CALCULATOR, "initial view");
    check(app.zoom == 4, "initial zoom");
    check(app.dirty == APP_DIRTY_ALL, "initial full redraw");
    render(&app);
    check(app.dirty == APP_DIRTY_NONE, "render consumes dirty regions");

    appSetExpression(&app, "1/2");
    render(&app);
    appHandleKeys(&app, APP_KEY_L, APP_KEY_L);
    check(app.cursor.offset == 2 &&
          app.cursor.slot == NATURAL_SLOT_DENOMINATOR,
          "shoulder cursor movement");
    appHandleKeys(&app, APP_KEY_SELECT | APP_KEY_UP, 0);
    check(app.cursor.slot == NATURAL_SLOT_NUMERATOR,
          "vertical structural cursor chord");

    render(&app);
    appHandleKeys(&app, APP_KEY_RIGHT, APP_KEY_RIGHT);
    check(app.selectedToken == 1, "keypad navigation");
    check((app.dirty & APP_DIRTY_KEYPAD) != 0 &&
          (app.dirty & APP_DIRTY_EXPRESSION) == 0,
          "key selection dirties keypad only");
    render(&app);
    appHandleKeys(&app, APP_KEY_B | APP_KEY_RIGHT, 0);
    check(app.tokenPage == 1, "keypad page chord");
    check(strcmp(app.expression, "1/2") == 0,
          "page chord does not backspace");

    app.mode = CALC_MODE_COMP;
    app.view = APP_VIEW_CALCULATOR;
    appSetExpression(&app, "1+1");
    appHandleKeys(&app, APP_KEY_START, APP_KEY_START);
    check(app.runtime.error == CALC_OK &&
          strcmp(app.runtime.result, "2") == 0,
          "COMP evaluation");

    appHandleKeys(&app, APP_KEY_SELECT | APP_KEY_START, 0);
    check(app.view == APP_VIEW_MENU, "open 12-mode menu");
    app.menuSelection = CALC_MODE_GRAPHING;
    appSetGraphExpression(&app, "y=x");
    app.view = APP_VIEW_MENU;
    appHandleKeys(&app, APP_KEY_A, APP_KEY_A);
    check(app.view == APP_VIEW_GRAPH_INPUT,
          "GRAPHING opens its expression entry");
    app.graphSelectedToken = GRAPH_KEYPAD_EXE_INDEX;
    appHandleKeys(&app, APP_KEY_A, APP_KEY_A);
    check(app.view == APP_VIEW_GRAPH &&
          app.graphFunctionCount == 1 && !app.graph.complete,
          "GRAPH EXE opens graph view");

    render(&app);
    check(app.graph.nextSample == 32,
          "first graph frame streams 32 samples");
    check(app.dirty == APP_DIRTY_NONE,
          "graph frame consumed redraw flags");
    render(&app);
    check(app.graph.nextSample == 64,
          "stream continues with no dirty flags");
    appHandleKeys(&app, APP_KEY_START, APP_KEY_START);
    check(app.graph.nextSample == 0 &&
          (app.dirty & APP_DIRTY_ALL) != APP_DIRTY_ALL,
          "replot clears graph regions, not the full framebuffer");

    appHandleKeys(&app, APP_KEY_SELECT | APP_KEY_UP, 0);
    check(app.traceActive, "legacy trace toggle");
    check(app.cursor.offset == GBA_SCREEN_WIDTH / 2,
          "trace begins at center");
    appHandleKeys(&app, APP_KEY_L, APP_KEY_L);
    check(app.cursor.offset == GBA_SCREEN_WIDTH / 2 - 1,
          "shoulder moves active trace");
    appHandleKeys(&app, APP_KEY_A, APP_KEY_A);
    check(!app.traceActive, "A toggles trace off");

    oldCamera = app.cameraX;
    appHandleKeys(&app, APP_KEY_RIGHT, APP_KEY_RIGHT);
    check(app.cameraX > oldCamera && app.graph.nextSample == 0,
          "graph pan restarts viewport stream");
    oldZoom = app.zoom;
    appHandleKeys(&app, APP_KEY_R, APP_KEY_R);
    check(app.zoom == oldZoom + 1, "graph zoom in");

    appHandleKeys(&app, APP_KEY_B, APP_KEY_B);
    check(app.view == APP_VIEW_GRAPH_INPUT &&
          strcmp(app.graphExpression, "y=x") == 0,
          "graph returns to GRAPH expression entry without text loss");
    app.mode = CALC_MODE_TABLE;
    app.view = APP_VIEW_CALCULATOR;
    appSetExpression(&app, "x^2;0;2;1");
    appHandleKeys(&app, APP_KEY_START, APP_KEY_START);
    check(app.view == APP_VIEW_TABLE && app.runtime.table.rows == 3,
          "table result view");
    check((app.dirty & APP_DIRTY_ALL) != APP_DIRTY_ALL,
          "table update uses owned regions, not full clear");
    render(&app);

    testSharedEditorKeypad();
    testModeEntryContracts();
    testSharedEditorSelectionClamps();
    testGraphInputKeypad();
    testMenuReturnViews();
    checkFramebufferGuards();

    if (failures != 0)
        return 1;
    puts("app_runtime_test: PASS");
    return 0;
}
