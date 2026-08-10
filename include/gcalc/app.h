#ifndef GCALC_APP_H
#define GCALC_APP_H

#include "gcalc/graph.h"
#include "gcalc/gfx.h"
#include "gcalc/modes.h"
#include "gcalc/natural.h"

#define APP_EXPRESSION_CAPACITY 127
#define APP_FORM_MAX_FIELDS 7
#define APP_FORM_FIELD_CAPACITY 31
#define APP_STAT_MAX_ROWS 64
#define APP_STAT_COLUMNS 3
#define APP_STAT_CELL_CAPACITY 15
#define APP_GRID_MAX_PANELS 2
#define APP_GRID_MAX_ROWS 4
#define APP_GRID_MAX_COLUMNS 7
#define APP_GRID_CELL_CAPACITY 15

#define APP_KEY_A      0x0001
#define APP_KEY_B      0x0002
#define APP_KEY_SELECT 0x0004
#define APP_KEY_START  0x0008
#define APP_KEY_RIGHT  0x0010
#define APP_KEY_LEFT   0x0020
#define APP_KEY_UP     0x0040
#define APP_KEY_DOWN   0x0080
#define APP_KEY_R      0x0100
#define APP_KEY_L      0x0200

typedef enum AppView {
    APP_VIEW_CALCULATOR,
    APP_VIEW_TABLE,
    APP_VIEW_GRAPH,
    APP_VIEW_MENU,
    APP_VIEW_GRAPH_INPUT,
    /* Mode-owned workspaces.  These are deliberately separate views rather
       than skins over the COMP editor: each one has its own navigation and
       state transition rules. */
    APP_VIEW_MODE_ACTION,
    APP_VIEW_MODE_FORM,
    APP_VIEW_MODE_GRID,
    APP_VIEW_STAT_TYPE,
    APP_VIEW_STAT_DATA,
    APP_VIEW_BASEN
} AppView;

typedef enum AppFormAction {
    APP_FORM_NONE,
    APP_FORM_EQN_SOLVE,
    APP_FORM_EQN_SOLVE_RANGE,
    APP_FORM_EQN_LINEAR_2,
    APP_FORM_EQN_LINEAR_3,
    APP_FORM_EQN_LINEAR_4,
    APP_FORM_EQN_POLY_2,
    APP_FORM_EQN_POLY_3,
    APP_FORM_EQN_POLY_4,
    APP_FORM_EQN_POLY_5,
    APP_FORM_EQN_POLY_6,
    APP_FORM_MATRIX_DET,
    APP_FORM_MATRIX_INV,
    APP_FORM_MATRIX_TRANSPOSE,
    APP_FORM_MATRIX_ADD,
    APP_FORM_MATRIX_SUB,
    APP_FORM_MATRIX_MUL,
    APP_FORM_TABLE_F,
    APP_FORM_TABLE_FG,
    APP_FORM_TABLE_DERIVATIVE,
    APP_FORM_VECTOR_NORM,
    APP_FORM_VECTOR_DOT,
    APP_FORM_VECTOR_CROSS,
    APP_FORM_VECTOR_ANGLE,
    APP_FORM_VECTOR_SCALE,
    APP_FORM_INEQ_LINEAR,
    APP_FORM_INEQ_QUADRATIC,
    APP_FORM_INEQ_CUBIC,
    APP_FORM_INEQ_QUARTIC,
    APP_FORM_RATIO_SIMPLIFY,
    APP_FORM_RATIO_PROPORTION,
    APP_FORM_DIST_BINOM_PDF,
    APP_FORM_DIST_BINOM_CDF,
    APP_FORM_DIST_POISSON_PDF,
    APP_FORM_DIST_POISSON_CDF,
    APP_FORM_DIST_NORMAL_PDF,
    APP_FORM_DIST_NORMAL_CDF,
    APP_FORM_DIST_NORMAL_INV,
    APP_FORM_DIST_GEOM_PDF,
    APP_FORM_DIST_GEOM_CDF,
    APP_FORM_DIST_HYPERGEOM,
    APP_FORM_MATRIX_EDIT_A,
    APP_FORM_MATRIX_EDIT_B,
    APP_FORM_MATRIX_EDIT_C,
    APP_FORM_MATRIX_EDIT_D,
    APP_FORM_VECTOR_EDIT_A,
    APP_FORM_VECTOR_EDIT_B,
    APP_FORM_VECTOR_EDIT_C,
    APP_FORM_VECTOR_EDIT_D
} AppFormAction;

typedef enum AppDirty {
    APP_DIRTY_NONE = 0,
    APP_DIRTY_HEADER = 1 << 0,
    APP_DIRTY_EXPRESSION = 1 << 1,
    APP_DIRTY_RESULT = 1 << 2,
    APP_DIRTY_STATUS = 1 << 3,
    APP_DIRTY_KEYPAD = 1 << 4,
    APP_DIRTY_VIEWPORT = 1 << 5,
    APP_DIRTY_ALL = 0x3f
} AppDirty;

typedef struct AppState {
    ModeRuntime runtime;
    GraphJob graph;
    GraphFunction graphFunctions[GRAPH_MAX_FUNCTIONS];
    NaturalCursor cursor;
    NaturalCursor graphCursor;
    NaturalCursor formCursor;
    NaturalCursor baseCursor;
    NaturalCursor gridCursor;
    char expression[APP_EXPRESSION_CAPACITY + 1];
    char graphExpression[APP_EXPRESSION_CAPACITY + 1];
    char baseExpression[APP_EXPRESSION_CAPACITY + 1];
    char formFields[APP_FORM_MAX_FIELDS]
                   [APP_FORM_FIELD_CAPACITY + 1];
    char statCells[APP_STAT_MAX_ROWS][APP_STAT_COLUMNS]
                  [APP_STAT_CELL_CAPACITY + 1];
    char gridCells[APP_GRID_MAX_PANELS][APP_GRID_MAX_ROWS]
                  [APP_GRID_MAX_COLUMNS][APP_GRID_CELL_CAPACITY + 1];
    char status[48];
    u8 expressionLength;
    u8 graphExpressionLength;
    u8 baseExpressionLength;
    u8 formLengths[APP_FORM_MAX_FIELDS];
    u8 statCellLengths[APP_STAT_MAX_ROWS][APP_STAT_COLUMNS];
    u8 gridCellLengths[APP_GRID_MAX_PANELS][APP_GRID_MAX_ROWS]
                      [APP_GRID_MAX_COLUMNS];
    u8 selectedToken;
    u8 tokenPage;
    u8 graphSelectedToken;
    u8 graphTokenPage;
    u8 menuSelection;
    u8 menuReturnSelection;
    u8 graphFunctionCount;
    u8 actionSelection;
    u8 formField;
    u8 formFieldCount;
    u8 formAction;
    u8 statRow;
    u8 statColumn;
    u8 statScroll;
    u8 gridPanel;
    u8 gridPanelCount;
    /* Each panel owns its dimensions.  MATRIX multiplication in particular
       needs A(r x k) and B(k x c), so a single shared shape would silently
       turn the cell editor into a square-matrix-only UI. */
    u8 gridRows[APP_GRID_MAX_PANELS];
    u8 gridColumns[APP_GRID_MAX_PANELS];
    u8 gridRow;
    u8 gridColumn;
    u8 gridRelation;
    u8 traceActive;
    u8 cursorVisible;
    CalcMode mode;
    AppView view;
    AppView menuReturnView;
    u32 dirty;
    u32 frame;
    s16 cameraX;
    s16 cameraY;
    u8 zoom;
} AppState;

void appInit(AppState *app);
void appSetExpression(AppState *app, const char *expression);
void appSetGraphExpression(AppState *app, const char *expression);
void appHandleKeys(AppState *app, u16 pressed, u16 held);
void appTick(AppState *app);
void appRender(AppState *app, GfxSurface *surface);

/* Introspection used by host verification and by platform front ends. */
AppView appModeEntryView(CalcMode mode);
u8 appModeKeypadPageCount(CalcMode mode);

#endif
