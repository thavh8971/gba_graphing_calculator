#include "gcalc/app.h"

#include <string.h>

#define APP_TOKEN_PAGES 15
#define APP_PAGE_SPECIAL_TOKENS 12
#define APP_TOKENS_PER_PAGE 30
#define APP_KEYPAD_EXE 29
#define APP_KEYPAD_COLUMNS 6
#define APP_KEYPAD_ROWS 5

#define APP_HEADER_Y 0
#define APP_HEADER_HEIGHT 14
#define APP_EXPRESSION_Y 14
#define APP_EXPRESSION_HEIGHT 38
#define APP_RESULT_Y 52
#define APP_RESULT_HEIGHT 20
#define APP_STATUS_Y 72
#define APP_STATUS_HEIGHT 10
#define APP_KEYPAD_Y 82
#define APP_KEYPAD_HEIGHT 78

#define APP_GRAPH_Y 14
#define APP_GRAPH_HEIGHT 134
#define APP_GRAPH_STATUS_Y 149
#define APP_GRAPH_STATUS_HEIGHT 11

#define APP_GRAPH_INPUT_EXPRESSION_Y 14
#define APP_GRAPH_INPUT_EXPRESSION_HEIGHT 36
#define APP_GRAPH_INPUT_STATUS_Y 50
#define APP_GRAPH_INPUT_STATUS_HEIGHT 10
#define APP_GRAPH_INPUT_KEYPAD_Y 60
#define APP_GRAPH_INPUT_KEYPAD_HEIGHT 100
#define APP_GRAPH_INPUT_COLUMNS 6
#define APP_GRAPH_INPUT_ROWS 5
#define APP_GRAPH_INPUT_TOKENS 30
#define APP_GRAPH_INPUT_EXE 29
#define APP_GRAPH_INPUT_PAGES 4
#define APP_BASE_PAGES 2

#define APP_FORM_Y 14
#define APP_FORM_FIELDS_Y 30
#define APP_FORM_FIELDS_HEIGHT 47
#define APP_FORM_RESULT_Y 77
#define APP_FORM_RESULT_HEIGHT 17
#define APP_FORM_KEYPAD_Y 94
#define APP_FORM_KEYPAD_HEIGHT 66

#define APP_STAT_TABLE_Y 14
#define APP_STAT_TABLE_HEIGHT 46
#define APP_STAT_STATUS_Y 60
#define APP_STAT_STATUS_HEIGHT 10
#define APP_STAT_KEYPAD_Y 70
#define APP_STAT_KEYPAD_HEIGHT 90

#define APP_GRID_TABLE_Y 24
#define APP_GRID_TABLE_HEIGHT 48
#define APP_GRID_RESULT_Y 72
#define APP_GRID_RESULT_HEIGHT 12
#define APP_GRID_STATUS_Y 84
#define APP_GRID_STATUS_HEIGHT 10
#define APP_GRID_KEYPAD_Y 94
#define APP_GRID_KEYPAD_HEIGHT 66

/* Textbook MATRIX/VECTOR editor geometry.  The bracket owns the full
   expression block; cells are deliberately borderless so the display reads
   as one matrix instead of a collection of independent spreadsheet boxes. */
#define APP_GRID_BRACKET_LEFT 7
#define APP_GRID_BRACKET_RIGHT 232
#define APP_GRID_BRACKET_CAP 6
#define APP_GRID_BRACKET_STROKE 2
#define APP_GRID_BRACKET_CONTENT_LEFT 14
#define APP_GRID_BRACKET_CONTENT_RIGHT 226
#define APP_GRID_TEXTBOOK_ROW_HEIGHT 10
#define APP_GRID_TEXTBOOK_MIN_HEIGHT 13

#define APP_ACTION_COLUMNS 2
#define APP_ACTION_TOP 18
#define APP_ACTION_CELL_HEIGHT 26

#define APP_COLOR_BACKGROUND RGB15(1, 2, 4)
#define APP_COLOR_PANEL RGB15(4, 6, 10)
#define APP_COLOR_PANEL_ALT RGB15(7, 9, 14)
#define APP_COLOR_TEXT RGB15(29, 30, 31)
#define APP_COLOR_MUTED RGB15(16, 19, 22)
#define APP_COLOR_ACCENT RGB15(5, 24, 31)
#define APP_COLOR_CURSOR RGB15(31, 27, 5)
#define APP_COLOR_ERROR RGB15(31, 8, 8)
#define APP_COLOR_GRID RGB15(7, 9, 12)
#define APP_COLOR_AXIS RGB15(17, 19, 22)

typedef struct AppToken {
    const char *label;
    const char *insert;
    u8 rewind;
} AppToken;

typedef struct NaturalBox {
    s16 width;
    s16 above;
    s16 below;
} NaturalBox;

typedef struct NaturalPainter {
    GfxSurface *surface;
    const char *text;
    u8 cursorOffset;
    s16 cursorX;
    s16 cursorY;
    u8 cursorFound;
    u16 color;
} NaturalPainter;

typedef struct NaturalSplit {
    s16 position;
    u8 width;
} NaturalSplit;

#define TOKEN(label_, insert_, rewind_) {label_, insert_, rewind_}

static const char *const appPageLabels[APP_TOKEN_PAGES] = {
    "NUM", "EDIT", "TRIG", "FUNC", "CALC", "VAR",
    "GRAPH", "CMPLX", "SOLVE", "LINALG", "DIST", "BASE", "CMD",
    "MATRIX", "VECTOR"
};

static const AppToken
appTokens[APP_TOKEN_PAGES][APP_PAGE_SPECIAL_TOKENS] = {
    {
        TOKEN("7", "7", 0), TOKEN("8", "8", 0),
        TOKEN("9", "9", 0), TOKEN("+", "+", 0),
        TOKEN("4", "4", 0), TOKEN("5", "5", 0),
        TOKEN("6", "6", 0), TOKEN("-", "-", 0),
        TOKEN("1", "1", 0), TOKEN("2", "2", 0),
        TOKEN("3", "3", 0), TOKEN("0", "0", 0)
    },
    {
        TOKEN(".", ".", 0), TOKEN(",", ",", 0),
        TOKEN("(", "(", 0), TOKEN(")", ")", 0),
        TOKEN("x", "x", 0), TOKEN("y", "y", 0),
        TOKEN("t", "t", 0), TOKEN("^", "^", 0),
        TOKEN("/", "/", 0), TOKEN("*", "*", 0),
        TOKEN("E", "E", 0), TOKEN(";", ";", 0)
    },
    {
        TOKEN("sin", "sin()", 1), TOKEN("cos", "cos()", 1),
        TOKEN("tan", "tan()", 1), TOKEN("asin", "asin()", 1),
        TOKEN("acos", "acos()", 1), TOKEN("atan", "atan()", 1),
        TOKEN("sinh", "sinh()", 1), TOKEN("cosh", "cosh()", 1),
        TOKEN("tanh", "tanh()", 1), TOKEN("sec", "sec()", 1),
        TOKEN("csc", "csc()", 1), TOKEN("cot", "cot()", 1)
    },
    {
        TOKEN("sqrt", "sqrt()", 1), TOKEN("root", "root(;)", 2),
        TOKEN("cbrt", "cbrt()", 1), TOKEN("ln", "ln()", 1),
        TOKEN("log", "log()", 1), TOKEN("exp", "exp()", 1),
        TOKEN("abs", "abs()", 1), TOKEN("floor", "floor()", 1),
        TOKEN("ceil", "ceil()", 1), TOKEN("trunc", "trunc()", 1),
        TOKEN("round", "round()", 1), TOKEN("fac", "fac()", 1)
    },
    {
        TOKEN("sum", "sum(;;;)", 4), TOKEN("prod", "prod(;;;)", 4),
        TOKEN("intgrl", "integral(;;;)", 4),
        TOKEN("d/dx", "d/dx(;;)", 3),
        TOKEN("d2/dx2", "d2/dx2(;;)", 3),
        TOKEN("GCD", "GCD(;)", 2), TOKEN("LCM", "LCM(;)", 2),
        TOKEN("MOD", "MOD(;)", 2), TOKEN("nPr", "nPr(;)", 2),
        TOKEN("nCr", "nCr(;)", 2), TOKEN("Ran#", "Ran#", 0),
        TOKEN("RanInt", "RanInt#(;)", 2)
    },
    {
        TOKEN("Ans", "Ans", 0), TOKEN("PreAns", "PreAns", 0),
        TOKEN("A", "A", 0), TOKEN("B", "B", 0),
        TOKEN("C", "C", 0), TOKEN("D", "D", 0),
        TOKEN("M", "M", 0), TOKEN("N", "N", 0),
        TOKEN("X", "X", 0), TOKEN("Y", "Y", 0),
        TOKEN("pi", "pi", 0), TOKEN("e", "e", 0)
    },
    {
        TOKEN("y=", "y=", 0), TOKEN("x=", "x=", 0),
        TOKEN("r=", "r=", 0), TOKEN("param", "param(;)", 2),
        TOKEN(";", ";", 0), TOKEN("=", "=", 0),
        TOKEN("<", "<", 0), TOKEN("<=", "<=", 0),
        TOKEN(">", ">", 0), TOKEN(">=", ">=", 0),
        TOKEN("!=", "!=", 0), TOKEN("t", "t", 0)
    },
    {
        TOKEN("i", "i", 0), TOKEN("conj", "conj()", 1),
        TOKEN("re", "re()", 1), TOKEN("im", "im()", 1),
        TOKEN("abs", "abs()", 1), TOKEN("norm", "norm()", 1),
        TOKEN("arg", "arg()", 1), TOKEN("polar", "polar(;)", 2),
        TOKEN("rect", "rect(;)", 2), TOKEN("pow", "pow(;)", 2),
        TOKEN("+", "+", 0), TOKEN(";", ";", 0)
    },
    {
        TOKEN("solve", "solve(;)", 2), TOKEN("solven", "solven(;;)", 3),
        TOKEN("lin", "lin(;)", 2), TOKEN("poly", "poly(;;)", 3),
        TOKEN("quad", "quad()", 1), TOKEN("freq", "statfreq(;)", 2),
        TOKEN("dtable", "dtable(;;;)", 4), TOKEN("=", "=", 0),
        TOKEN(";", ";", 0), TOKEN(":", ":", 0),
        TOKEN("[", "[", 0), TOKEN("]", "]", 0)
    },
    {
        TOKEN("det", "det()", 1), TOKEN("inv", "inv()", 1),
        TOKEN("trans", "transpose()", 1), TOKEN("add", "add(;)", 2),
        TOKEN("sub", "sub(;)", 2), TOKEN("mul", "mul(;)", 2),
        TOKEN("norm", "norm()", 1), TOKEN("dot", "dot(;)", 2),
        TOKEN("cross", "cross(;)", 2), TOKEN("angle", "angle(;)", 2),
        TOKEN("scale", "scale(;)", 2), TOKEN(";", ";", 0)
    },
    {
        TOKEN("BinPDF", "binompdf(;;)", 3),
        TOKEN("BinCDF", "binomcdf(;;)", 3),
        TOKEN("PoiPDF", "poissonpdf(;)", 2),
        TOKEN("PoiCDF", "poissoncdf(;)", 2),
        TOKEN("NorPDF", "normpdf(;;)", 3),
        TOKEN("NorCDF", "normcdf(;;;)", 4),
        TOKEN("NorInv", "norminv(;;)", 3),
        TOKEN("GeoPDF", "geompdf(;)", 2),
        TOKEN("GeoCDF", "geomcdf(;)", 2),
        TOKEN("Hyper", "hypergeom(;;;)", 4),
        TOKEN(";", ";", 0), TOKEN(",", ",", 0)
    },
    {
        TOKEN("BIN", "bin()", 1), TOKEN("OCT", "oct()", 1),
        TOKEN("DEC", "dec()", 1), TOKEN("HEX", "hex()", 1),
        TOKEN("and", "and(;)", 2), TOKEN("or", "or(;)", 2),
        TOKEN("xor", "xor(;)", 2), TOKEN("not", "not()", 1),
        TOKEN("shl", "shl(;)", 2), TOKEN("shr", "shr(;)", 2),
        TOKEN("A-F", "A", 0), TOKEN(";", ";", 0)
    },
    {
        TOKEN("Solve", "solve()", 1), TOKEN("Calc", "calc()", 1),
        TOKEN("d/dx", "d/dx(;;)", 3), TOKEN("Int", "integral(;;;)", 4),
        TOKEN("Sum", "sum(;;;)", 4), TOKEN("Prod", "prod(;;;)", 4),
        TOKEN("nPr", "nPr(;)", 2), TOKEN("nCr", "nCr(;)", 2),
        TOKEN("GCD", "GCD(;)", 2), TOKEN("LCM", "LCM(;)", 2),
        TOKEN("MOD", "MOD(;)", 2), TOKEN("Ran#", "Ran#", 0)
    },
    {
        TOKEN("MatA", "MatA", 0), TOKEN("MatB", "MatB", 0),
        TOKEN("MatC", "MatC", 0), TOKEN("MatD", "MatD", 0),
        TOKEN("det", "det()", 1), TOKEN("inv", "inv()", 1),
        TOKEN("trans", "transpose()", 1), TOKEN("add", "add(;)", 2),
        TOKEN("sub", "sub(;)", 2), TOKEN("mul", "mul(;)", 2),
        TOKEN("[", "[", 0), TOKEN("]", "]", 0)
    },
    {
        TOKEN("VctA", "VctA", 0), TOKEN("VctB", "VctB", 0),
        TOKEN("VctC", "VctC", 0), TOKEN("VctD", "VctD", 0),
        TOKEN("norm", "norm()", 1), TOKEN("dot", "dot(;)", 2),
        TOKEN("cross", "cross(;)", 2), TOKEN("angle", "angle(;)", 2),
        TOKEN("scale", "scale(;)", 2), TOKEN("[", "[", 0),
        TOKEN("]", "]", 0), TOKEN(";", ";", 0)
    }
};

/* The first twelve cells remain page-specific.  Cells 12..28 form a common
   editing strip so every shared page has 29 live tokens, and cell 29 is EXE. */
static const AppToken appCommonTokens[17] = {
    TOKEN(".", ".", 0), TOKEN("(", "(", 0), TOKEN(")", ")", 0),
    TOKEN("*", "*", 0), TOKEN("/", "/", 0), TOKEN("^", "^", 0),
    TOKEN("x", "x", 0), TOKEN("y", "y", 0), TOKEN("t", "t", 0),
    TOKEN(";", ";", 0), TOKEN(",", ",", 0), TOKEN("=", "=", 0),
    TOKEN(":", ":", 0), TOKEN("[", "[", 0), TOKEN("]", "]", 0),
    TOKEN("Ans", "Ans", 0), TOKEN("pi", "pi", 0)
};

static const AppToken appExecuteToken = TOKEN("EXE", "", 0);

/* Logical keypad pages are selected per sub-application.  A mode never sees
   the command catalog of an unrelated mode. */
static const u8 appCompPages[] = {0, 1, 2, 3, 4, 5, 12};
static const u8 appComplexPages[] = {0, 1, 2, 3, 4, 5, 12, 7};
static const u8 appEquationPages[] = {0, 1, 3, 5};
static const u8 appMatrixPages[] = {0, 1, 13};
static const u8 appTablePages[] = {0, 1, 2, 3, 5};
static const u8 appVectorPages[] = {0, 1, 14};
static const u8 appInequalityPages[] = {0, 1, 3, 5};
static const u8 appRatioPages[] = {0, 1};
static const u8 appDistributionPages[] = {0, 1, 3};
static const u8 appStatPages[] = {0, 1, 3};

typedef struct AppActionSpec {
    AppFormAction action;
    const char *label;
    const char *call;
    u8 fieldCount;
    const char *field[APP_FORM_MAX_FIELDS];
} AppActionSpec;

#define ACTION0(id_, label_, call_) \
    {id_, label_, call_, 0, {0, 0, 0, 0, 0, 0, 0}}
#define ACTION1(id_, label_, call_, f0_) \
    {id_, label_, call_, 1, {f0_, 0, 0, 0, 0, 0, 0}}
#define ACTION2(id_, label_, call_, f0_, f1_) \
    {id_, label_, call_, 2, {f0_, f1_, 0, 0, 0, 0, 0}}
#define ACTION3(id_, label_, call_, f0_, f1_, f2_) \
    {id_, label_, call_, 3, {f0_, f1_, f2_, 0, 0, 0, 0}}
#define ACTION4(id_, label_, call_, f0_, f1_, f2_, f3_) \
    {id_, label_, call_, 4, {f0_, f1_, f2_, f3_, 0, 0, 0}}
#define ACTION5(id_, label_, call_, f0_, f1_, f2_, f3_, f4_) \
    {id_, label_, call_, 5, {f0_, f1_, f2_, f3_, f4_, 0, 0}}
#define ACTION6(id_, label_, call_, f0_, f1_, f2_, f3_, f4_, f5_) \
    {id_, label_, call_, 6, {f0_, f1_, f2_, f3_, f4_, f5_, 0}}
#define ACTION7(id_, label_, call_, f0_, f1_, f2_, f3_, f4_, f5_, f6_) \
    {id_, label_, call_, 7, {f0_, f1_, f2_, f3_, f4_, f5_, f6_}}

static const AppActionSpec appEquationActions[] = {
    ACTION2(APP_FORM_EQN_LINEAR_2, "2 UNKN", "lin", "ROW 1", "ROW 2"),
    ACTION3(APP_FORM_EQN_LINEAR_3, "3 UNKN", "lin",
            "ROW 1", "ROW 2", "ROW 3"),
    ACTION4(APP_FORM_EQN_LINEAR_4, "4 UNKN", "lin",
            "ROW 1", "ROW 2", "ROW 3", "ROW 4"),
    ACTION3(APP_FORM_EQN_POLY_2, "POLY 2", "poly", "A", "B", "C"),
    ACTION4(APP_FORM_EQN_POLY_3, "POLY 3", "poly",
            "A", "B", "C", "D"),
    ACTION5(APP_FORM_EQN_POLY_4, "POLY 4", "poly",
            "A", "B", "C", "D", "E"),
    ACTION6(APP_FORM_EQN_POLY_5, "POLY 5", "poly",
            "A", "B", "C", "D", "E", "F"),
    ACTION7(APP_FORM_EQN_POLY_6, "POLY 6", "poly",
            "A", "B", "C", "D", "E", "F", "G")
};

static const AppActionSpec appMatrixActions[] = {
    ACTION0(APP_FORM_MATRIX_EDIT_A, "EDIT MatA", ""),
    ACTION0(APP_FORM_MATRIX_EDIT_B, "EDIT MatB", ""),
    ACTION0(APP_FORM_MATRIX_EDIT_C, "EDIT MatC", ""),
    ACTION0(APP_FORM_MATRIX_EDIT_D, "EDIT MatD", ""),
    ACTION1(APP_FORM_MATRIX_DET, "det()", "det", "MAT A"),
    ACTION1(APP_FORM_MATRIX_INV, "inv()", "inv", "MAT A"),
    ACTION1(APP_FORM_MATRIX_TRANSPOSE, "transpose()", "transpose", "MAT A"),
    ACTION2(APP_FORM_MATRIX_ADD, "add()", "add", "MAT A", "MAT B"),
    ACTION2(APP_FORM_MATRIX_SUB, "sub()", "sub", "MAT A", "MAT B"),
    ACTION2(APP_FORM_MATRIX_MUL, "mul()", "mul", "MAT A", "MAT B")
};

static const AppActionSpec appTableActions[] = {
    ACTION4(APP_FORM_TABLE_F, "F(X)", "", "F(X)", "START", "END", "STEP"),
    ACTION5(APP_FORM_TABLE_FG, "F(X),G(X)", "", "F(X)", "G(X)",
            "START", "END", "STEP"),
    ACTION4(APP_FORM_TABLE_DERIVATIVE, "F + DERIV", "dtable",
            "F(X)", "START", "END", "STEP")
};

static const AppActionSpec appVectorActions[] = {
    ACTION0(APP_FORM_VECTOR_EDIT_A, "EDIT VctA", ""),
    ACTION0(APP_FORM_VECTOR_EDIT_B, "EDIT VctB", ""),
    ACTION0(APP_FORM_VECTOR_EDIT_C, "EDIT VctC", ""),
    ACTION0(APP_FORM_VECTOR_EDIT_D, "EDIT VctD", ""),
    ACTION1(APP_FORM_VECTOR_NORM, "norm()", "norm", "VECTOR A"),
    ACTION2(APP_FORM_VECTOR_DOT, "dot()", "dot", "VECTOR A", "VECTOR B"),
    ACTION2(APP_FORM_VECTOR_CROSS, "cross()", "cross", "VECTOR A", "VECTOR B"),
    ACTION2(APP_FORM_VECTOR_ANGLE, "angle()", "angle", "VECTOR A", "VECTOR B"),
    ACTION2(APP_FORM_VECTOR_SCALE, "scale()", "scale", "SCALAR", "VECTOR A")
};

static const AppActionSpec appInequalityActions[] = {
    ACTION0(APP_FORM_INEQ_QUADRATIC, "DEGREE 2", "ineq2"),
    ACTION0(APP_FORM_INEQ_CUBIC, "DEGREE 3", "ineq3"),
    ACTION0(APP_FORM_INEQ_QUARTIC, "DEGREE 4", "ineq4")
};

static const AppActionSpec appRatioActions[] = {
    ACTION3(APP_FORM_RATIO_SIMPLIFY, "A:B = X:D", "ratio1",
            "A", "B", "D"),
    ACTION3(APP_FORM_RATIO_PROPORTION, "A:B = C:X", "ratio2",
            "A", "B", "C")
};

static const AppActionSpec appDistributionActions[] = {
    ACTION3(APP_FORM_DIST_BINOM_PDF, "BINOM PDF", "binompdf", "K", "N", "P"),
    ACTION3(APP_FORM_DIST_BINOM_CDF, "BINOM CDF", "binomcdf", "K", "N", "P"),
    ACTION2(APP_FORM_DIST_POISSON_PDF, "POISSON PDF", "poissonpdf", "K", "LAMBDA"),
    ACTION2(APP_FORM_DIST_POISSON_CDF, "POISSON CDF", "poissoncdf", "K", "LAMBDA"),
    ACTION3(APP_FORM_DIST_NORMAL_PDF, "NORMAL PDF", "normpdf", "X", "SD", "MEAN"),
    ACTION4(APP_FORM_DIST_NORMAL_CDF, "NORMAL CDF", "normcdf",
            "LOWER", "UPPER", "SD", "MEAN"),
    ACTION3(APP_FORM_DIST_NORMAL_INV, "NORMAL INV", "norminv", "P", "SD", "MEAN")
};

static const AppActionSpec *appActionSpecs(CalcMode mode, u8 *count)
{
    const AppActionSpec *specs = 0;
    *count = 0;
    switch (mode) {
    case CALC_MODE_EQN:
        specs = appEquationActions;
        *count = GCALC_ARRAY_COUNT(appEquationActions);
        break;
    case CALC_MODE_MATRIX:
        specs = appMatrixActions;
        *count = GCALC_ARRAY_COUNT(appMatrixActions);
        break;
    case CALC_MODE_TABLE:
        specs = appTableActions;
        *count = GCALC_ARRAY_COUNT(appTableActions);
        break;
    case CALC_MODE_VECTOR:
        specs = appVectorActions;
        *count = GCALC_ARRAY_COUNT(appVectorActions);
        break;
    case CALC_MODE_INEQ:
        specs = appInequalityActions;
        *count = GCALC_ARRAY_COUNT(appInequalityActions);
        break;
    case CALC_MODE_RATIO:
        specs = appRatioActions;
        *count = GCALC_ARRAY_COUNT(appRatioActions);
        break;
    case CALC_MODE_DIST:
        specs = appDistributionActions;
        *count = GCALC_ARRAY_COUNT(appDistributionActions);
        break;
    default:
        break;
    }
    return specs;
}

static const AppActionSpec *appCurrentAction(const AppState *app)
{
    const AppActionSpec *specs;
    u8 count;
    u8 index;

    specs = appActionSpecs(app->mode, &count);
    for (index = 0; index < count; index++)
        if ((u8)specs[index].action == app->formAction)
            return &specs[index];
    return 0;
}

static u8 appMatrixEditRegister(AppFormAction action,
                                ModeNamedRegister *name)
{
    switch (action) {
    case APP_FORM_MATRIX_EDIT_A: *name = MODE_REGISTER_A; return 1;
    case APP_FORM_MATRIX_EDIT_B: *name = MODE_REGISTER_B; return 1;
    case APP_FORM_MATRIX_EDIT_C: *name = MODE_REGISTER_C; return 1;
    case APP_FORM_MATRIX_EDIT_D: *name = MODE_REGISTER_D; return 1;
    default: return 0;
    }
}

static u8 appVectorEditRegister(AppFormAction action,
                                ModeNamedRegister *name)
{
    switch (action) {
    case APP_FORM_VECTOR_EDIT_A: *name = MODE_REGISTER_A; return 1;
    case APP_FORM_VECTOR_EDIT_B: *name = MODE_REGISTER_B; return 1;
    case APP_FORM_VECTOR_EDIT_C: *name = MODE_REGISTER_C; return 1;
    case APP_FORM_VECTOR_EDIT_D: *name = MODE_REGISTER_D; return 1;
    default: return 0;
    }
}

static u8 appNamedGridRegister(const AppState *app,
                               ModeNamedRegister *name)
{
    AppFormAction action = (AppFormAction)app->formAction;

    if (app->mode == CALC_MODE_MATRIX)
        return appMatrixEditRegister(action, name);
    if (app->mode == CALC_MODE_VECTOR)
        return appVectorEditRegister(action, name);
    return 0;
}

static const u8 *appModePages(CalcMode mode, u8 *count)
{
    const u8 *pages;

    switch (mode) {
    case CALC_MODE_CMPLX:
        pages = appComplexPages;
        *count = GCALC_ARRAY_COUNT(appComplexPages);
        break;
    case CALC_MODE_EQN:
        pages = appEquationPages;
        *count = GCALC_ARRAY_COUNT(appEquationPages);
        break;
    case CALC_MODE_MATRIX:
        pages = appMatrixPages;
        *count = GCALC_ARRAY_COUNT(appMatrixPages);
        break;
    case CALC_MODE_TABLE:
        pages = appTablePages;
        *count = GCALC_ARRAY_COUNT(appTablePages);
        break;
    case CALC_MODE_VECTOR:
        pages = appVectorPages;
        *count = GCALC_ARRAY_COUNT(appVectorPages);
        break;
    case CALC_MODE_INEQ:
        pages = appInequalityPages;
        *count = GCALC_ARRAY_COUNT(appInequalityPages);
        break;
    case CALC_MODE_RATIO:
        pages = appRatioPages;
        *count = GCALC_ARRAY_COUNT(appRatioPages);
        break;
    case CALC_MODE_DIST:
        pages = appDistributionPages;
        *count = GCALC_ARRAY_COUNT(appDistributionPages);
        break;
    case CALC_MODE_STAT:
        pages = appStatPages;
        *count = GCALC_ARRAY_COUNT(appStatPages);
        break;
    case CALC_MODE_COMP:
    default:
        pages = appCompPages;
        *count = GCALC_ARRAY_COUNT(appCompPages);
        break;
    }
    return pages;
}

u8 appModeKeypadPageCount(CalcMode mode)
{
    u8 count;
    if (mode == CALC_MODE_BASEN)
        return APP_BASE_PAGES;
    if (mode == CALC_MODE_GRAPHING)
        return APP_GRAPH_INPUT_PAGES;
    (void)appModePages(mode, &count);
    return count;
}

static u8 appModePageId(CalcMode mode, u8 logicalPage)
{
    u8 count;
    const u8 *pages = appModePages(mode, &count);
    return pages[logicalPage < count ? logicalPage : 0];
}

static const char *appModePageLabel(CalcMode mode, u8 logicalPage)
{
    return appPageLabels[appModePageId(mode, logicalPage)];
}

static const char *const appGraphPageLabels[APP_GRAPH_INPUT_PAGES] = {
    "PLOT", "FUNC", "CALC", "SYM"
};

/* GRAPH has its own dense 6x5 keypad.  Slot 29 is deliberately EXE on every
   page; it is an action and is never inserted into the source buffer. */
static const AppToken
appGraphTokens[APP_GRAPH_INPUT_PAGES][APP_GRAPH_INPUT_TOKENS] = {
    {
        TOKEN("y=", "y=", 0), TOKEN("x=", "x=", 0),
        TOKEN("r=", "r=", 0), TOKEN("param", "param(;)", 2),
        TOKEN(":", ":", 0), TOKEN(";", ";", 0),
        TOKEN("7", "7", 0), TOKEN("8", "8", 0),
        TOKEN("9", "9", 0), TOKEN("(", "(", 0),
        TOKEN(")", ")", 0), TOKEN("^", "^", 0),
        TOKEN("4", "4", 0), TOKEN("5", "5", 0),
        TOKEN("6", "6", 0), TOKEN("*", "*", 0),
        TOKEN("/", "/", 0), TOKEN("+", "+", 0),
        TOKEN("1", "1", 0), TOKEN("2", "2", 0),
        TOKEN("3", "3", 0), TOKEN("-", "-", 0),
        TOKEN(".", ".", 0), TOKEN(",", ",", 0),
        TOKEN("0", "0", 0), TOKEN("x", "x", 0),
        TOKEN("t", "t", 0), TOKEN("=", "=", 0),
        TOKEN("<=", "<=", 0), TOKEN("EXE", "", 0)
    },
    {
        TOKEN("sin", "sin()", 1), TOKEN("cos", "cos()", 1),
        TOKEN("tan", "tan()", 1), TOKEN("asin", "asin()", 1),
        TOKEN("acos", "acos()", 1), TOKEN("atan", "atan()", 1),
        TOKEN("sinh", "sinh()", 1), TOKEN("cosh", "cosh()", 1),
        TOKEN("tanh", "tanh()", 1), TOKEN("sqrt", "sqrt()", 1),
        TOKEN("root", "root(;)", 2), TOKEN("cbrt", "cbrt()", 1),
        TOKEN("ln", "ln()", 1), TOKEN("log", "log()", 1),
        TOKEN("exp", "exp()", 1), TOKEN("abs", "abs()", 1),
        TOKEN("floor", "floor()", 1), TOKEN("ceil", "ceil()", 1),
        TOKEN("sec", "sec()", 1), TOKEN("csc", "csc()", 1),
        TOKEN("cot", "cot()", 1), TOKEN("nroot", "nroot(;)", 2),
        TOKEN("pow", "pow(;)", 2), TOKEN("fac", "fac()", 1),
        TOKEN("x", "x", 0), TOKEN("t", "t", 0),
        TOKEN("pi", "pi", 0), TOKEN("e", "e", 0),
        TOKEN("Ans", "Ans", 0), TOKEN("EXE", "", 0)
    },
    {
        TOKEN("sum", "sum(;;;)", 4),
        TOKEN("prod", "prod(;;;)", 4),
        TOKEN("intgrl", "integral(;;;)", 4),
        TOKEN("d/dx", "d/dx(;;)", 3),
        TOKEN("d2/dx2", "d2/dx2(;;)", 3),
        TOKEN("diff", "diff(;;)", 3),
        TOKEN("GCD", "GCD(;)", 2), TOKEN("LCM", "LCM(;)", 2),
        TOKEN("MOD", "MOD(;)", 2), TOKEN("nPr", "nPr(;)", 2),
        TOKEN("nCr", "nCr(;)", 2), TOKEN("log10", "log10()", 1),
        TOKEN("int", "int()", 1), TOKEN("intg", "intg()", 1),
        TOKEN("frac", "frac()", 1), TOKEN("round", "round()", 1),
        TOKEN("trunc", "trunc()", 1), TOKEN("ceil", "ceil()", 1),
        TOKEN("Ran#", "Ran#", 0), TOKEN("RanInt", "RanInt#(;)", 2),
        TOKEN("root", "root(;)", 2), TOKEN("abs", "abs()", 1),
        TOKEN("fac", "fac()", 1), TOKEN("exp", "exp()", 1),
        TOKEN("x", "x", 0), TOKEN("t", "t", 0),
        TOKEN("A", "A", 0), TOKEN("B", "B", 0),
        TOKEN("pi", "pi", 0), TOKEN("EXE", "", 0)
    },
    {
        TOKEN("=", "=", 0), TOKEN("!=", "!=", 0),
        TOKEN("<", "<", 0), TOKEN("<=", "<=", 0),
        TOKEN(">", ">", 0), TOKEN(">=", ">=", 0),
        TOKEN("+", "+", 0), TOKEN("-", "-", 0),
        TOKEN("*", "*", 0), TOKEN("/", "/", 0),
        TOKEN("^", "^", 0), TOKEN("E", "E", 0),
        TOKEN("(", "(", 0), TOKEN(")", ")", 0),
        TOKEN("[", "[", 0), TOKEN("]", "]", 0),
        TOKEN(";", ";", 0), TOKEN(",", ",", 0),
        TOKEN("x", "x", 0), TOKEN("y", "y", 0),
        TOKEN("t", "t", 0), TOKEN("A", "A", 0),
        TOKEN("B", "B", 0), TOKEN("C", "C", 0),
        TOKEN("pi", "pi", 0), TOKEN("e", "e", 0),
        TOKEN("Ans", "Ans", 0), TOKEN("y=", "y=", 0),
        TOKEN("param", "param(;)", 2), TOKEN("EXE", "", 0)
    }
};

static const char *const appBasePageLabels[APP_BASE_PAGES] = {
    "DIGITS", "LOGIC"
};

static const AppToken appBaseTokens[APP_BASE_PAGES][APP_TOKENS_PER_PAGE] = {
    {
        TOKEN("DEC", "", 0), TOKEN("HEX", "", 0),
        TOKEN("BIN", "", 0), TOKEN("OCT", "", 0),
        TOKEN("(", "(", 0), TOKEN(")", ")", 0),
        TOKEN("A", "A", 0), TOKEN("B", "B", 0),
        TOKEN("C", "C", 0), TOKEN("D", "D", 0),
        TOKEN("E", "E", 0), TOKEN("F", "F", 0),
        TOKEN("7", "7", 0), TOKEN("8", "8", 0),
        TOKEN("9", "9", 0), TOKEN("+", "+", 0),
        TOKEN("-", "-", 0), TOKEN("*", "*", 0),
        TOKEN("4", "4", 0), TOKEN("5", "5", 0),
        TOKEN("6", "6", 0), TOKEN("/", "/", 0),
        TOKEN("and", "and(;)", 2), TOKEN("or", "or(;)", 2),
        TOKEN("1", "1", 0), TOKEN("2", "2", 0),
        TOKEN("3", "3", 0), TOKEN("0", "0", 0),
        TOKEN("Neg", "neg()", 1), TOKEN("EXE", "", 0)
    },
    {
        TOKEN("DEC", "", 0), TOKEN("HEX", "", 0),
        TOKEN("BIN", "", 0), TOKEN("OCT", "", 0),
        TOKEN("bin", "bin()", 1), TOKEN("hex", "hex()", 1),
        TOKEN("oct", "oct()", 1), TOKEN("dec", "dec()", 1),
        TOKEN("and", "and(;)", 2), TOKEN("or", "or(;)", 2),
        TOKEN("xor", "xor(;)", 2), TOKEN("xnor", "xnor(;)", 2),
        TOKEN("not", "not()", 1), TOKEN("Neg", "neg()", 1),
        TOKEN("A", "A", 0), TOKEN("B", "B", 0),
        TOKEN("C", "C", 0), TOKEN("D", "D", 0),
        TOKEN("E", "E", 0), TOKEN("F", "F", 0),
        TOKEN("7", "7", 0), TOKEN("8", "8", 0),
        TOKEN("9", "9", 0), TOKEN("shl", "shl(;)", 2),
        TOKEN("1", "1", 0), TOKEN("2", "2", 0),
        TOKEN("3", "3", 0), TOKEN("shr", "shr(;)", 2),
        TOKEN("0", "0", 0), TOKEN("EXE", "", 0)
    }
};

static const AppToken *appSharedToken(CalcMode mode, u8 page, u8 index)
{
    page = appModePageId(mode, page);
    if (index < APP_PAGE_SPECIAL_TOKENS)
        return &appTokens[page][index];
    if (index < APP_KEYPAD_EXE)
        return &appCommonTokens[index - APP_PAGE_SPECIAL_TOKENS];
    return &appExecuteToken;
}

static const u16 appGraphColors[GRAPH_MAX_FUNCTIONS] = {
    RGB15(31, 9, 7), RGB15(5, 27, 31), RGB15(10, 31, 9),
    RGB15(31, 22, 4), RGB15(24, 9, 31), RGB15(31, 14, 24),
    RGB15(9, 18, 31), RGB15(18, 31, 20), RGB15(31, 31, 12),
    RGB15(24, 24, 31)
};

static u8 appIsSpace(char value)
{
    return (u8)(value == ' ' || value == '\t' || value == '\r' ||
                value == '\n');
}

static char appLower(char value)
{
    if (value >= 'A' && value <= 'Z')
        return (char)(value + ('a' - 'A'));
    return value;
}

static void appCopyText(char *destination, u16 capacity, const char *source)
{
    u16 index = 0;

    if (destination == 0 || capacity == 0)
        return;
    if (source != 0) {
        while (source[index] != '\0' && index + 1 < capacity) {
            destination[index] = source[index];
            index++;
        }
    }
    destination[index] = '\0';
}

static void appAppendText(char *destination, u16 capacity,
                          const char *source)
{
    u16 length = 0;
    u16 index = 0;

    if (destination == 0 || source == 0 || capacity == 0)
        return;
    while (length + 1 < capacity && destination[length] != '\0')
        length++;
    while (length + 1 < capacity && source[index] != '\0')
        destination[length++] = source[index++];
    destination[length] = '\0';
}

static u8 appAppendChecked(char *destination, u16 capacity,
                           const char *source);

static void appSetStatus(AppState *app, const char *status)
{
    appCopyText(app->status, sizeof(app->status), status);
    app->dirty |= APP_DIRTY_STATUS;
}

static const char *appSlotLabel(NaturalSlot slot)
{
    switch (slot) {
    case NATURAL_SLOT_NUMERATOR: return "NUMERATOR";
    case NATURAL_SLOT_DENOMINATOR: return "DENOMINATOR";
    case NATURAL_SLOT_BASE: return "BASE";
    case NATURAL_SLOT_EXPONENT: return "EXPONENT";
    case NATURAL_SLOT_INDEX: return "INDEX";
    case NATURAL_SLOT_RADICAND: return "RADICAND";
    case NATURAL_SLOT_BODY: return "BODY";
    case NATURAL_SLOT_VARIABLE: return "VARIABLE";
    case NATURAL_SLOT_LOWER: return "LOWER";
    case NATURAL_SLOT_UPPER: return "UPPER";
    case NATURAL_SLOT_EVALUATION: return "AT";
    case NATURAL_SLOT_MAIN:
    default: return "MAIN";
    }
}

static void appUpdateEditStatus(AppState *app)
{
    char page[5];
    u8 number = (u8)(app->tokenPage + 1);

    appCopyText(app->status, sizeof(app->status),
                appSlotLabel(app->cursor.slot));
    appAppendText(app->status, sizeof(app->status), "  ");
    appAppendText(app->status, sizeof(app->status),
                  appModePageLabel(app->mode, app->tokenPage));
    page[0] = ' ';
    page[1] = 'P';
    if (number >= 10) {
        page[2] = '1';
        page[3] = (char)('0' + number - 10);
        page[4] = '\0';
    } else {
        page[2] = (char)('0' + number);
        page[3] = '\0';
    }
    appAppendText(app->status, sizeof(app->status), page);
    app->dirty |= APP_DIRTY_STATUS;
}

static void appUpdateGraphInputStatus(AppState *app)
{
    char page[5];
    u8 number = (u8)(app->graphTokenPage + 1);

    appCopyText(app->status, sizeof(app->status),
                appSlotLabel(app->graphCursor.slot));
    appAppendText(app->status, sizeof(app->status), "  ");
    appAppendText(app->status, sizeof(app->status),
                  appGraphPageLabels[app->graphTokenPage]);
    page[0] = ' ';
    page[1] = 'P';
    page[2] = (char)('0' + number);
    page[3] = '\0';
    appAppendText(app->status, sizeof(app->status), page);
    app->dirty |= APP_DIRTY_STATUS;
}

static void appUpdateFormStatus(AppState *app)
{
    const AppActionSpec *spec = appCurrentAction(app);
    const char *field = spec != 0 && app->formField < spec->fieldCount ?
                        spec->field[app->formField] : "FIELD";

    appCopyText(app->status, sizeof(app->status), field);
    appAppendText(app->status, sizeof(app->status), "  ");
    appAppendText(app->status, sizeof(app->status),
                  appModePageLabel(app->mode, app->tokenPage));
    app->dirty |= APP_DIRTY_STATUS;
}

static void appUpdateBaseStatus(AppState *app)
{
    appCopyText(app->status, sizeof(app->status), "RADIX ");
    appAppendText(app->status, sizeof(app->status),
                  modeBaseRadixLabel(modeGetBaseRadix(&app->runtime)));
    appAppendText(app->status, sizeof(app->status), "  ");
    appAppendText(app->status, sizeof(app->status),
                  appBasePageLabels[app->tokenPage < APP_BASE_PAGES ?
                                    app->tokenPage : 0]);
    app->dirty |= APP_DIRTY_STATUS;
}

static void appUpdateStatStatus(AppState *app)
{
    appCopyText(app->status, sizeof(app->status),
                modeStatModelLabel(modeGetStatModel(&app->runtime)));
    appAppendText(app->status, sizeof(app->status), "  SEL+DPAD:CELL");
    app->dirty |= APP_DIRTY_STATUS;
}

static const char *appGridPanelLabel(const AppState *app)
{
    ModeNamedRegister name;

    if (app->mode == CALC_MODE_MATRIX &&
        appNamedGridRegister(app, &name))
        return modeMatrixRegisterLabel(name);
    if (app->mode == CALC_MODE_VECTOR &&
        appNamedGridRegister(app, &name))
        return modeVectorRegisterLabel(name);
    if (app->mode == CALC_MODE_MATRIX)
        return app->gridPanel == 0 ? "MAT A" : "MAT B";
    if (app->mode == CALC_MODE_VECTOR) {
        if (app->formAction == APP_FORM_VECTOR_SCALE &&
            app->gridPanel == 0)
            return "K";
        return app->gridPanel == 0 ||
               app->formAction == APP_FORM_VECTOR_SCALE ? "VEC A" :
                                                          "VEC B";
    }
    return "COEFF";
}

static const char *appGridRelationLabel(u8 relation)
{
    static const char *const labels[4] = {"<", ">", "<=", ">="};
    return labels[relation < 4 ? relation : 0];
}

static void appUpdateGridStatus(AppState *app)
{
    char cell[8];
    ModeNamedRegister name;

    appCopyText(app->status, sizeof(app->status), appGridPanelLabel(app));
    cell[0] = ' ';
    cell[1] = 'R';
    cell[2] = (char)('1' + app->gridRow);
    cell[3] = 'C';
    cell[4] = (char)('1' + app->gridColumn);
    cell[5] = '\0';
    appAppendText(app->status, sizeof(app->status), cell);
    if (app->mode == CALC_MODE_INEQ) {
        appAppendText(app->status, sizeof(app->status), "  OP ");
        appAppendText(app->status, sizeof(app->status),
                      appGridRelationLabel(app->gridRelation));
    } else if (appNamedGridRegister(app, &name)) {
        appAppendText(app->status, sizeof(app->status),
                      "  B:SAVE SEL+B:DEL");
    }
    app->dirty |= APP_DIRTY_STATUS;
}

static void appUpdateKeypadStatus(AppState *app)
{
    if (app->view == APP_VIEW_MODE_FORM)
        appUpdateFormStatus(app);
    else if (app->view == APP_VIEW_MODE_GRID)
        appUpdateGridStatus(app);
    else if (app->view == APP_VIEW_STAT_DATA)
        appUpdateStatStatus(app);
    else if (app->view == APP_VIEW_BASEN)
        appUpdateBaseStatus(app);
    else
        appUpdateEditStatus(app);
}

static s32 appClampS16(s32 value)
{
    if (value < -32768)
        return -32768;
    if (value > 32767)
        return 32767;
    return value;
}

static GraphViewport appViewport(const AppState *app)
{
    GraphViewport viewport;
    s32 halfWidth = (10L * CALC_ONE * 16L) >> app->zoom;
    s32 halfHeight;

    if (halfWidth < 8)
        halfWidth = 8;
    halfHeight = (s32)(((s64)halfWidth * APP_GRAPH_HEIGHT) /
                       GBA_SCREEN_WIDTH);
    if (halfHeight < 8)
        halfHeight = 8;
    viewport.xMin = (s32)app->cameraX - halfWidth;
    viewport.xMax = (s32)app->cameraX + halfWidth;
    viewport.yMin = (s32)app->cameraY - halfHeight;
    viewport.yMax = (s32)app->cameraY + halfHeight;
    return viewport;
}

static void appRestartGraph(AppState *app)
{
    GraphViewport viewport = appViewport(app);

    graphJobBegin(&app->graph, &app->runtime.calc, app->graphFunctions,
                  app->graphFunctionCount, viewport, GBA_SCREEN_WIDTH);
    app->dirty |= APP_DIRTY_VIEWPORT | APP_DIRTY_STATUS;
}

static u8 appStartGraph(AppState *app)
{
    u8 error;
    u8 count = 0;
    u8 alreadyGraph = app->view == APP_VIEW_GRAPH;

    error = graphParseFunctions(app->graphExpression, app->graphFunctions,
                                GRAPH_MAX_FUNCTIONS, &count);
    app->runtime.error = error;
    if (error != CALC_OK) {
        app->runtime.result[0] = '\0';
        appCopyText(app->runtime.result, sizeof(app->runtime.result),
                    calcErrorText(error));
        appSetStatus(app, "PLOT ERROR");
        app->dirty |= APP_DIRTY_RESULT;
        return error;
    }

    app->graphFunctionCount = count;
    app->traceActive = 0;
    app->view = APP_VIEW_GRAPH;
    appCopyText(app->status, sizeof(app->status), "PLOTTING");
    appRestartGraph(app);
    if (!alreadyGraph)
        app->dirty = APP_DIRTY_ALL;
    return CALC_OK;
}

static void appEvaluate(AppState *app)
{
    u8 error;

    if (app->mode == CALC_MODE_GRAPHING) {
        appStartGraph(app);
        return;
    }
    error = modeEvaluate(&app->runtime, app->mode, app->expression);
    if (error == CALC_OK)
        appSetStatus(app, "DONE");
    else
        appSetStatus(app, calcErrorText(error));
    app->dirty |= APP_DIRTY_RESULT;
    if (app->mode == CALC_MODE_TABLE && error == CALC_OK) {
        app->selectedToken = 0;
        app->view = APP_VIEW_TABLE;
        /* Changing table contents clears only its owned screen regions. */
        app->dirty = APP_DIRTY_HEADER | APP_DIRTY_VIEWPORT |
                     APP_DIRTY_STATUS;
    }
}

AppView appModeEntryView(CalcMode mode)
{
    switch (mode) {
    case CALC_MODE_COMP:
    case CALC_MODE_CMPLX:
        return APP_VIEW_CALCULATOR;
    case CALC_MODE_STAT:
        return APP_VIEW_STAT_TYPE;
    case CALC_MODE_BASEN:
        return APP_VIEW_BASEN;
    case CALC_MODE_GRAPHING:
        return APP_VIEW_GRAPH_INPUT;
    case CALC_MODE_EQN:
    case CALC_MODE_MATRIX:
    case CALC_MODE_TABLE:
    case CALC_MODE_VECTOR:
    case CALC_MODE_INEQ:
    case CALC_MODE_RATIO:
    case CALC_MODE_DIST:
        return APP_VIEW_MODE_ACTION;
    default:
        return APP_VIEW_CALCULATOR;
    }
}

void appInit(AppState *app)
{
    if (app == 0)
        return;
    memset(app, 0, sizeof(*app));
    modeRuntimeInit(&app->runtime);
    app->mode = CALC_MODE_COMP;
    app->view = APP_VIEW_CALCULATOR;
    app->menuReturnView = APP_VIEW_CALCULATOR;
    app->zoom = 4;
    app->cursorVisible = 1;
    app->menuSelection = 0xff;
    naturalCursorSetEnd(&app->cursor, 0);
    naturalCursorSetEnd(&app->graphCursor, 0);
    naturalCursorSetEnd(&app->formCursor, 0);
    naturalCursorSetEnd(&app->baseCursor, 0);
    naturalCursorSetEnd(&app->gridCursor, 0);
    modeSetBaseRadix(&app->runtime, BASE_RADIX_DEC);
    modeSetStatModel(&app->runtime, STAT_MODEL_1VAR);
    appUpdateEditStatus(app);
    app->dirty = APP_DIRTY_ALL;
}

void appSetExpression(AppState *app, const char *expression)
{
    u16 length = 0;

    if (app == 0)
        return;
    if (expression != 0) {
        while (expression[length] != '\0' &&
               length < APP_EXPRESSION_CAPACITY) {
            app->expression[length] = expression[length];
            length++;
        }
    }
    app->expression[length] = '\0';
    app->expressionLength = (u8)length;
    naturalCursorSetEnd(&app->cursor, app->expressionLength);
    app->runtime.result[0] = '\0';
    app->runtime.error = CALC_OK;
    app->cursorVisible = 1;
    appUpdateEditStatus(app);
    app->dirty |= APP_DIRTY_EXPRESSION | APP_DIRTY_RESULT |
                  APP_DIRTY_STATUS;
}

void appSetGraphExpression(AppState *app, const char *expression)
{
    u16 length = 0;

    if (app == 0)
        return;
    if (expression != 0) {
        while (expression[length] != '\0' &&
               length < APP_EXPRESSION_CAPACITY) {
            app->graphExpression[length] = expression[length];
            length++;
        }
    }
    app->graphExpression[length] = '\0';
    app->graphExpressionLength = (u8)length;
    naturalCursorSetEnd(&app->graphCursor, app->graphExpressionLength);
    app->runtime.error = CALC_OK;
    app->cursorVisible = 1;
    appUpdateGraphInputStatus(app);
    app->dirty |= APP_DIRTY_EXPRESSION | APP_DIRTY_STATUS;
}

static u8 appChord(u16 pressed, u16 held, u16 first, u16 second)
{
    return (u8)(((pressed & first) != 0 && (held & second) != 0) ||
                ((pressed & second) != 0 && (held & first) != 0) ||
                ((pressed & (first | second)) == (first | second)));
}

static void appMoveKeypad(AppState *app, s8 horizontal, s8 vertical)
{
    u8 pageCount = appModeKeypadPageCount(app->mode);
    u8 old = app->selectedToken;
    u8 row = (u8)(old / APP_KEYPAD_COLUMNS);
    u8 column = (u8)(old % APP_KEYPAD_COLUMNS);
    u8 pageChanged = 0;

    if (old >= APP_TOKENS_PER_PAGE) {
        old = 0;
        row = 0;
        column = 0;
    }
    if (horizontal < 0) {
        if (column == 0) {
            column = APP_KEYPAD_COLUMNS - 1;
            app->tokenPage = (u8)((app->tokenPage + pageCount - 1) %
                                  pageCount);
            pageChanged = 1;
        } else
            column--;
    } else if (horizontal > 0) {
        if (column + 1 == APP_KEYPAD_COLUMNS) {
            column = 0;
            app->tokenPage = (u8)((app->tokenPage + 1) % pageCount);
            pageChanged = 1;
        } else
            column++;
    } else if (vertical < 0) {
        if (row == 0) {
            row = APP_KEYPAD_ROWS - 1;
            app->tokenPage = (u8)((app->tokenPage + pageCount - 1) %
                                  pageCount);
            pageChanged = 1;
        } else
            row--;
    } else if (vertical > 0) {
        if (row + 1 == APP_KEYPAD_ROWS) {
            row = 0;
            app->tokenPage = (u8)((app->tokenPage + 1) % pageCount);
            pageChanged = 1;
        } else
            row++;
    }
    app->selectedToken = (u8)(row * APP_KEYPAD_COLUMNS + column);
    if (pageChanged) {
        app->menuSelection = 0xff;
        appUpdateKeypadStatus(app);
        app->dirty |= APP_DIRTY_HEADER;
    } else
        app->menuSelection = old;
    if (app->selectedToken != old || pageChanged)
        app->dirty |= APP_DIRTY_KEYPAD;
}

static void appChangePage(AppState *app, s8 direction)
{
    u8 pageCount = appModeKeypadPageCount(app->mode);
    if (direction < 0)
        app->tokenPage = (u8)((app->tokenPage + pageCount - 1) %
                              pageCount);
    else
        app->tokenPage = (u8)((app->tokenPage + 1) % pageCount);
    app->menuSelection = 0xff;
    appUpdateKeypadStatus(app);
    app->dirty |= APP_DIRTY_KEYPAD | APP_DIRTY_HEADER;
}

static void appInsertSelected(AppState *app)
{
    const AppToken *token;
    u8 rewind;

    if (app->selectedToken >= APP_TOKENS_PER_PAGE)
        app->selectedToken = 0;
    if (app->selectedToken == APP_KEYPAD_EXE) {
        appEvaluate(app);
        return;
    }
    token = appSharedToken(app->mode, app->tokenPage, app->selectedToken);

    if (!naturalCursorInsert(app->expression, &app->expressionLength,
                             APP_EXPRESSION_CAPACITY, token->insert,
                             &app->cursor)) {
        appSetStatus(app, "EXPRESSION FULL");
        return;
    }
    rewind = token->rewind;
    while (rewind-- != 0)
        naturalCursorMoveHorizontal(app->expression, app->expressionLength,
                                    &app->cursor, -1);
    app->runtime.result[0] = '\0';
    app->cursorVisible = 1;
    appUpdateEditStatus(app);
    app->dirty |= APP_DIRTY_EXPRESSION | APP_DIRTY_RESULT;
}

static void appHandleCalculatorKeys(AppState *app, u16 pressed, u16 held)
{
    if (appChord(pressed, held, APP_KEY_B, APP_KEY_LEFT)) {
        appChangePage(app, -1);
        return;
    }
    if (appChord(pressed, held, APP_KEY_B, APP_KEY_RIGHT)) {
        appChangePage(app, 1);
        return;
    }
    if (appChord(pressed, held, APP_KEY_SELECT, APP_KEY_UP)) {
        naturalCursorMoveVertical(app->expression, app->expressionLength,
                                  &app->cursor, -1);
        app->cursorVisible = 1;
        appUpdateEditStatus(app);
        app->dirty |= APP_DIRTY_EXPRESSION;
        return;
    }
    if (appChord(pressed, held, APP_KEY_SELECT, APP_KEY_DOWN)) {
        naturalCursorMoveVertical(app->expression, app->expressionLength,
                                  &app->cursor, 1);
        app->cursorVisible = 1;
        appUpdateEditStatus(app);
        app->dirty |= APP_DIRTY_EXPRESSION;
        return;
    }
    if ((pressed & APP_KEY_L) != 0) {
        naturalCursorMoveHorizontal(app->expression, app->expressionLength,
                                    &app->cursor, -1);
        app->cursorVisible = 1;
        appUpdateEditStatus(app);
        app->dirty |= APP_DIRTY_EXPRESSION;
    }
    if ((pressed & APP_KEY_R) != 0) {
        naturalCursorMoveHorizontal(app->expression, app->expressionLength,
                                    &app->cursor, 1);
        app->cursorVisible = 1;
        appUpdateEditStatus(app);
        app->dirty |= APP_DIRTY_EXPRESSION;
    }
    if ((held & APP_KEY_SELECT) == 0) {
        if ((pressed & APP_KEY_LEFT) != 0)
            appMoveKeypad(app, -1, 0);
        else if ((pressed & APP_KEY_RIGHT) != 0)
            appMoveKeypad(app, 1, 0);
        else if ((pressed & APP_KEY_UP) != 0)
            appMoveKeypad(app, 0, -1);
        else if ((pressed & APP_KEY_DOWN) != 0)
            appMoveKeypad(app, 0, 1);
    }
    if ((pressed & APP_KEY_A) != 0)
        appInsertSelected(app);
    if ((pressed & APP_KEY_B) != 0) {
        if (naturalCursorBackspace(app->expression, &app->expressionLength,
                                   &app->cursor)) {
            app->runtime.result[0] = '\0';
            app->cursorVisible = 1;
            appUpdateEditStatus(app);
            app->dirty |= APP_DIRTY_EXPRESSION | APP_DIRTY_RESULT;
        }
    }
    if ((pressed & APP_KEY_START) != 0)
        appEvaluate(app);
}

static u8 appInsertBufferToken(char *buffer, u8 *length, u8 capacity,
                               NaturalCursor *cursor,
                               const AppToken *token)
{
    u8 rewind;

    if (!naturalCursorInsert(buffer, length, capacity, token->insert,
                             cursor))
        return 0;
    rewind = token->rewind;
    while (rewind-- != 0)
        naturalCursorMoveHorizontal(buffer, *length, cursor, -1);
    return 1;
}

static void appSetDefaultFormField(AppState *app, u8 field,
                                   const char *value)
{
    if (field >= APP_FORM_MAX_FIELDS || value == 0)
        return;
    appCopyText(app->formFields[field],
                sizeof(app->formFields[field]), value);
    app->formLengths[field] = (u8)strlen(app->formFields[field]);
}

static void appPrefillNamedRegisterForm(AppState *app,
                                        AppFormAction action)
{
    switch (action) {
    case APP_FORM_MATRIX_DET:
    case APP_FORM_MATRIX_INV:
    case APP_FORM_MATRIX_TRANSPOSE:
        appSetDefaultFormField(app, 0,
                               modeMatrixRegisterLabel(MODE_REGISTER_A));
        break;
    case APP_FORM_MATRIX_ADD:
    case APP_FORM_MATRIX_SUB:
    case APP_FORM_MATRIX_MUL:
        appSetDefaultFormField(app, 0,
                               modeMatrixRegisterLabel(MODE_REGISTER_A));
        appSetDefaultFormField(app, 1,
                               modeMatrixRegisterLabel(MODE_REGISTER_B));
        break;
    case APP_FORM_VECTOR_NORM:
        appSetDefaultFormField(app, 0,
                               modeVectorRegisterLabel(MODE_REGISTER_A));
        break;
    case APP_FORM_VECTOR_DOT:
    case APP_FORM_VECTOR_CROSS:
    case APP_FORM_VECTOR_ANGLE:
        appSetDefaultFormField(app, 0,
                               modeVectorRegisterLabel(MODE_REGISTER_A));
        appSetDefaultFormField(app, 1,
                               modeVectorRegisterLabel(MODE_REGISTER_B));
        break;
    case APP_FORM_VECTOR_SCALE:
        appSetDefaultFormField(app, 0, "1");
        appSetDefaultFormField(app, 1,
                               modeVectorRegisterLabel(MODE_REGISTER_A));
        break;
    default:
        break;
    }
}

static void appBeginForm(AppState *app)
{
    const AppActionSpec *specs;
    const AppActionSpec *spec;
    u8 count;
    u8 reset;

    specs = appActionSpecs(app->mode, &count);
    if (specs == 0 || count == 0)
        return;
    if (app->actionSelection >= count)
        app->actionSelection = 0;
    spec = &specs[app->actionSelection];
    reset = app->formAction != (u8)spec->action;
    if (reset) {
        memset(app->formFields, 0, sizeof(app->formFields));
        memset(app->formLengths, 0, sizeof(app->formLengths));
    }
    app->formAction = (u8)spec->action;
    if (reset)
        appPrefillNamedRegisterForm(app, spec->action);
    app->formFieldCount = spec->fieldCount;
    app->formField = 0;
    app->tokenPage = 0;
    app->selectedToken = 0;
    naturalCursorSetEnd(&app->formCursor, app->formLengths[0]);
    app->runtime.result[0] = '\0';
    app->runtime.error = CALC_OK;
    app->view = APP_VIEW_MODE_FORM;
    app->cursorVisible = 1;
    appUpdateFormStatus(app);
    app->dirty = APP_DIRTY_ALL;
}

static void appSetGridShape(AppState *app, u8 panel, u8 rows, u8 columns)
{
    if (panel >= APP_GRID_MAX_PANELS)
        return;
    if (rows == 0)
        rows = 1;
    if (columns == 0)
        columns = 1;
    if (rows > APP_GRID_MAX_ROWS)
        rows = APP_GRID_MAX_ROWS;
    if (columns > APP_GRID_MAX_COLUMNS)
        columns = APP_GRID_MAX_COLUMNS;
    app->gridRows[panel] = rows;
    app->gridColumns[panel] = columns;
}

static void appLoadNamedGridRegister(AppState *app)
{
    ModeNamedRegister name;
    u8 row;
    u8 column;

    if (!appNamedGridRegister(app, &name))
        return;
    memset(app->gridCells[0], 0, sizeof(app->gridCells[0]));
    memset(app->gridCellLengths[0], 0,
           sizeof(app->gridCellLengths[0]));
    if (app->mode == CALC_MODE_MATRIX) {
        ModeMatrixRegister stored;

        if (!modeMatrixGetRegister(&app->runtime, name, &stored)) {
            appSetGridShape(app, 0, 4, 4);
            return;
        }
        appSetGridShape(app, 0, stored.rows, stored.columns);
        for (row = 0; row < stored.rows; row++) {
            for (column = 0; column < stored.columns; column++) {
                calcFormatNumber(stored.cell[row][column],
                    app->gridCells[0][row][column],
                    sizeof(app->gridCells[0][row][column]));
                app->gridCellLengths[0][row][column] =
                    (u8)strlen(app->gridCells[0][row][column]);
            }
        }
    } else {
        ModeVectorRegister stored;

        if (!modeVectorGetRegister(&app->runtime, name, &stored)) {
            appSetGridShape(app, 0, 1, 3);
            return;
        }
        appSetGridShape(app, 0, 1, stored.dimensions);
        for (column = 0; column < stored.dimensions; column++) {
            calcFormatNumber(stored.component[column],
                app->gridCells[0][0][column],
                sizeof(app->gridCells[0][0][column]));
            app->gridCellLengths[0][0][column] =
                (u8)strlen(app->gridCells[0][0][column]);
        }
    }
}

static void appBeginGrid(AppState *app)
{
    const AppActionSpec *specs;
    const AppActionSpec *spec;
    u8 count;
    u8 oldAction = app->formAction;

    specs = appActionSpecs(app->mode, &count);
    if (specs == 0 || count == 0)
        return;
    if (app->actionSelection >= count)
        app->actionSelection = 0;
    spec = &specs[app->actionSelection];
    if (oldAction != (u8)spec->action) {
        memset(app->gridCells, 0, sizeof(app->gridCells));
        memset(app->gridCellLengths, 0, sizeof(app->gridCellLengths));
    }
    app->formAction = (u8)spec->action;
    app->gridPanelCount = 1;
    appSetGridShape(app, 0, 1, 1);
    appSetGridShape(app, 1, 1, 1);

    switch (spec->action) {
    case APP_FORM_EQN_LINEAR_2: appSetGridShape(app, 0, 2, 3); break;
    case APP_FORM_EQN_LINEAR_3: appSetGridShape(app, 0, 3, 4); break;
    case APP_FORM_EQN_LINEAR_4: appSetGridShape(app, 0, 4, 5); break;
    case APP_FORM_EQN_POLY_2: appSetGridShape(app, 0, 1, 3); break;
    case APP_FORM_EQN_POLY_3: appSetGridShape(app, 0, 1, 4); break;
    case APP_FORM_EQN_POLY_4: appSetGridShape(app, 0, 1, 5); break;
    case APP_FORM_EQN_POLY_5: appSetGridShape(app, 0, 1, 6); break;
    case APP_FORM_EQN_POLY_6: appSetGridShape(app, 0, 1, 7); break;
    case APP_FORM_MATRIX_EDIT_A:
    case APP_FORM_MATRIX_EDIT_B:
    case APP_FORM_MATRIX_EDIT_C:
    case APP_FORM_MATRIX_EDIT_D:
        appSetGridShape(app, 0, 4, 4);
        break;
    case APP_FORM_MATRIX_DET:
    case APP_FORM_MATRIX_INV:
    case APP_FORM_MATRIX_TRANSPOSE:
        appSetGridShape(app, 0, 2, 2);
        break;
    case APP_FORM_MATRIX_ADD:
    case APP_FORM_MATRIX_SUB:
    case APP_FORM_MATRIX_MUL:
        app->gridPanelCount = 2;
        appSetGridShape(app, 0, 2, 2);
        appSetGridShape(app, 1, 2, 2);
        break;
    case APP_FORM_VECTOR_NORM:
        appSetGridShape(app, 0, 1, 3);
        break;
    case APP_FORM_VECTOR_DOT:
    case APP_FORM_VECTOR_CROSS:
    case APP_FORM_VECTOR_ANGLE:
        app->gridPanelCount = 2;
        appSetGridShape(app, 0, 1, 3);
        appSetGridShape(app, 1, 1, 3);
        break;
    case APP_FORM_VECTOR_SCALE:
        app->gridPanelCount = 2;
        appSetGridShape(app, 0, 1, 1);
        appSetGridShape(app, 1, 1, 3);
        break;
    case APP_FORM_VECTOR_EDIT_A:
    case APP_FORM_VECTOR_EDIT_B:
    case APP_FORM_VECTOR_EDIT_C:
    case APP_FORM_VECTOR_EDIT_D:
        appSetGridShape(app, 0, 1, 3);
        break;
    case APP_FORM_INEQ_QUADRATIC:
        appSetGridShape(app, 0, 1, 3);
        break;
    case APP_FORM_INEQ_CUBIC:
        appSetGridShape(app, 0, 1, 4);
        break;
    case APP_FORM_INEQ_QUARTIC:
        appSetGridShape(app, 0, 1, 5);
        break;
    default:
        return;
    }
    appLoadNamedGridRegister(app);
    app->gridPanel = 0;
    app->gridRow = 0;
    app->gridColumn = 0;
    app->tokenPage = 0;
    app->selectedToken = 0;
    naturalCursorSetEnd(&app->gridCursor,
                        app->gridCellLengths[0][0][0]);
    app->runtime.result[0] = '\0';
    app->runtime.error = CALC_OK;
    app->view = APP_VIEW_MODE_GRID;
    app->cursorVisible = 1;
    appUpdateGridStatus(app);
    app->dirty = APP_DIRTY_ALL;
}

static void appBeginActionWorkspace(AppState *app)
{
    const AppActionSpec *specs;
    ModeNamedRegister name;
    u8 count;
    AppFormAction action;

    specs = appActionSpecs(app->mode, &count);
    if (specs == 0 || count == 0)
        return;
    if (app->actionSelection >= count)
        app->actionSelection = 0;
    action = specs[app->actionSelection].action;
    if (app->mode == CALC_MODE_EQN || app->mode == CALC_MODE_INEQ ||
        appMatrixEditRegister(action, &name) ||
        appVectorEditRegister(action, &name))
        appBeginGrid(app);
    else
        appBeginForm(app);
}

static u8 appBuildFormExpression(const AppState *app, char *output,
                                 u16 capacity)
{
    const AppActionSpec *spec = appCurrentAction(app);
    u8 field;

    if (spec == 0 || capacity == 0)
        return 0;
    output[0] = '\0';
    for (field = 0; field < spec->fieldCount; field++)
        if (app->formLengths[field] == 0)
            return 0;

    if (spec->call[0] != '\0') {
        appCopyText(output, capacity, spec->call);
        appAppendText(output, capacity, "(");
    }
    for (field = 0; field < spec->fieldCount; field++) {
        if (field != 0)
            appAppendText(output, capacity, ";");
        appAppendText(output, capacity, app->formFields[field]);
    }
    if (spec->call[0] != '\0')
        appAppendText(output, capacity, ")");
    return (u8)(strlen(output) + 1 < capacity);
}

static void appEvaluateForm(AppState *app)
{
    char source[APP_EXPRESSION_CAPACITY + 1];
    u8 error;

    if (!appBuildFormExpression(app, source, sizeof(source))) {
        appSetStatus(app, "COMPLETE ALL FIELDS");
        return;
    }
    appCopyText(app->expression, sizeof(app->expression), source);
    app->expressionLength = (u8)strlen(app->expression);
    error = modeEvaluate(&app->runtime, app->mode, source);
    if (error == CALC_OK)
        appSetStatus(app, "DONE");
    else
        appSetStatus(app, calcErrorText(error));
    app->dirty |= APP_DIRTY_RESULT;
    if (app->mode == CALC_MODE_TABLE && error == CALC_OK) {
        app->selectedToken = 0;
        app->view = APP_VIEW_TABLE;
        app->dirty = APP_DIRTY_HEADER | APP_DIRTY_VIEWPORT |
                     APP_DIRTY_STATUS;
    }
}

static void appMoveFormField(AppState *app, s8 direction)
{
    if (app->formFieldCount == 0)
        return;
    if (direction < 0)
        app->formField = (u8)((app->formField + app->formFieldCount - 1) %
                              app->formFieldCount);
    else
        app->formField = (u8)((app->formField + 1) % app->formFieldCount);
    naturalCursorSetEnd(&app->formCursor,
                        app->formLengths[app->formField]);
    app->cursorVisible = 1;
    appUpdateFormStatus(app);
    app->dirty |= APP_DIRTY_EXPRESSION;
}

static void appInsertFormSelected(AppState *app)
{
    const AppToken *token;

    if (app->selectedToken == APP_KEYPAD_EXE) {
        appEvaluateForm(app);
        return;
    }
    token = appSharedToken(app->mode, app->tokenPage,
                           app->selectedToken);
    if (!appInsertBufferToken(app->formFields[app->formField],
                              &app->formLengths[app->formField],
                              APP_FORM_FIELD_CAPACITY,
                              &app->formCursor, token)) {
        appSetStatus(app, "FIELD FULL");
        return;
    }
    app->runtime.result[0] = '\0';
    app->runtime.error = CALC_OK;
    app->cursorVisible = 1;
    appUpdateFormStatus(app);
    app->dirty |= APP_DIRTY_EXPRESSION | APP_DIRTY_RESULT;
}

static void appHandleActionKeys(AppState *app, u16 pressed)
{
    const AppActionSpec *specs;
    u8 count;
    u8 old = app->actionSelection;
    u8 row;
    u8 column;
    u8 rows;

    specs = appActionSpecs(app->mode, &count);
    if (specs == 0 || count == 0)
        return;
    if (old >= count)
        old = 0;
    row = (u8)(old / APP_ACTION_COLUMNS);
    column = (u8)(old % APP_ACTION_COLUMNS);
    rows = (u8)((count + APP_ACTION_COLUMNS - 1) / APP_ACTION_COLUMNS);
    if ((pressed & APP_KEY_LEFT) != 0)
        column = (u8)((column + APP_ACTION_COLUMNS - 1) %
                      APP_ACTION_COLUMNS);
    else if ((pressed & APP_KEY_RIGHT) != 0)
        column = (u8)((column + 1) % APP_ACTION_COLUMNS);
    else if ((pressed & APP_KEY_UP) != 0)
        row = (u8)((row + rows - 1) % rows);
    else if ((pressed & APP_KEY_DOWN) != 0)
        row = (u8)((row + 1) % rows);
    app->actionSelection = (u8)(row * APP_ACTION_COLUMNS + column);
    if (app->actionSelection >= count)
        app->actionSelection = (u8)(count - 1);
    if (app->actionSelection != old) {
        app->selectedToken = old;
        app->dirty |= APP_DIRTY_KEYPAD;
    }
    if ((pressed & (APP_KEY_A | APP_KEY_START)) != 0)
        appBeginActionWorkspace(app);
}

static void appHandleFormKeys(AppState *app, u16 pressed, u16 held)
{
    if (appChord(pressed, held, APP_KEY_B, APP_KEY_LEFT)) {
        appChangePage(app, -1);
        return;
    }
    if (appChord(pressed, held, APP_KEY_B, APP_KEY_RIGHT)) {
        appChangePage(app, 1);
        return;
    }
    if (appChord(pressed, held, APP_KEY_SELECT, APP_KEY_UP)) {
        appMoveFormField(app, -1);
        return;
    }
    if (appChord(pressed, held, APP_KEY_SELECT, APP_KEY_DOWN)) {
        appMoveFormField(app, 1);
        return;
    }
    if ((pressed & APP_KEY_L) != 0) {
        naturalCursorMoveHorizontal(app->formFields[app->formField],
                                    app->formLengths[app->formField],
                                    &app->formCursor, -1);
        app->cursorVisible = 1;
        app->dirty |= APP_DIRTY_EXPRESSION;
    }
    if ((pressed & APP_KEY_R) != 0) {
        naturalCursorMoveHorizontal(app->formFields[app->formField],
                                    app->formLengths[app->formField],
                                    &app->formCursor, 1);
        app->cursorVisible = 1;
        app->dirty |= APP_DIRTY_EXPRESSION;
    }
    if ((held & APP_KEY_SELECT) == 0) {
        if ((pressed & APP_KEY_LEFT) != 0)
            appMoveKeypad(app, -1, 0);
        else if ((pressed & APP_KEY_RIGHT) != 0)
            appMoveKeypad(app, 1, 0);
        else if ((pressed & APP_KEY_UP) != 0)
            appMoveKeypad(app, 0, -1);
        else if ((pressed & APP_KEY_DOWN) != 0)
            appMoveKeypad(app, 0, 1);
    }
    if ((pressed & APP_KEY_A) != 0)
        appInsertFormSelected(app);
    if ((pressed & APP_KEY_B) != 0) {
        if (!naturalCursorBackspace(app->formFields[app->formField],
                                    &app->formLengths[app->formField],
                                    &app->formCursor)) {
            app->view = APP_VIEW_MODE_ACTION;
            app->dirty = APP_DIRTY_ALL;
            return;
        }
        app->runtime.result[0] = '\0';
        app->cursorVisible = 1;
        app->dirty |= APP_DIRTY_EXPRESSION | APP_DIRTY_RESULT;
    }
    if ((pressed & APP_KEY_START) != 0)
        appEvaluateForm(app);
}

static u8 appAppendGridCell(const AppState *app, char *output,
                            u16 capacity, u8 panel, u8 row, u8 column)
{
    if (panel >= app->gridPanelCount ||
        row >= app->gridRows[panel] ||
        column >= app->gridColumns[panel] ||
        app->gridCellLengths[panel][row][column] == 0)
        return 0;
    return appAppendChecked(output, capacity,
                            app->gridCells[panel][row][column]);
}

static u8 appAppendGridPanel(const AppState *app, char *output,
                             u16 capacity, u8 panel, u8 brackets)
{
    u8 row;
    u8 column;

    if (brackets && !appAppendChecked(output, capacity, "["))
        return 0;
    for (row = 0; row < app->gridRows[panel]; row++) {
        if (row != 0 && !appAppendChecked(output, capacity, ";"))
            return 0;
        for (column = 0; column < app->gridColumns[panel]; column++) {
            if (column != 0 && !appAppendChecked(output, capacity, ","))
                return 0;
            if (!appAppendGridCell(app, output, capacity, panel, row,
                                   column))
                return 0;
        }
    }
    return !brackets || appAppendChecked(output, capacity, "]");
}

static u8 appBuildGridExpression(const AppState *app, char *output,
                                 u16 capacity)
{
    const AppActionSpec *spec = appCurrentAction(app);
    u8 row;
    u8 column;
    u8 panel;

    if (spec == 0 || capacity == 0 || spec->call[0] == '\0')
        return 0;
    output[0] = '\0';
    if (!appAppendChecked(output, capacity, spec->call) ||
        !appAppendChecked(output, capacity, "("))
        return 0;

    if (app->mode == CALC_MODE_EQN) {
        for (row = 0; row < app->gridRows[0]; row++) {
            for (column = 0; column < app->gridColumns[0]; column++) {
                const char *separator;
                if (row == 0 && column == 0)
                    separator = "";
                else if (spec->action >= APP_FORM_EQN_POLY_2 &&
                         spec->action <= APP_FORM_EQN_POLY_6)
                    separator = ";";
                else
                    separator = column == 0 ? ";" : ",";
                if (!appAppendChecked(output, capacity, separator) ||
                    !appAppendGridCell(app, output, capacity, 0, row,
                                       column))
                    return 0;
            }
        }
    } else if (app->mode == CALC_MODE_MATRIX) {
        for (panel = 0; panel < app->gridPanelCount; panel++) {
            if (panel != 0 && !appAppendChecked(output, capacity, "|"))
                return 0;
            if (!appAppendGridPanel(app, output, capacity, panel, 1))
                return 0;
        }
    } else if (app->mode == CALC_MODE_VECTOR) {
        for (panel = 0; panel < app->gridPanelCount; panel++) {
            if (panel != 0 && !appAppendChecked(output, capacity, ";"))
                return 0;
            if (!appAppendGridPanel(app, output, capacity, panel, 0))
                return 0;
        }
    } else if (app->mode == CALC_MODE_INEQ) {
        for (column = 0; column < app->gridColumns[0]; column++) {
            if (column != 0 && !appAppendChecked(output, capacity, ";"))
                return 0;
            if (!appAppendGridCell(app, output, capacity, 0, 0, column))
                return 0;
        }
        if (!appAppendChecked(output, capacity, ";") ||
            !appAppendChecked(output, capacity,
                              appGridRelationLabel(app->gridRelation)))
            return 0;
    } else
        return 0;
    return appAppendChecked(output, capacity, ")");
}

static u8 appEvaluateNamedGridCell(AppState *app,
                                   CalcContext *evaluationContext,
                                   u8 row, u8 column,
                                   CalcNumber *value)
{
    u8 error = CALC_OK;

    if (app->gridCellLengths[0][row][column] == 0) {
        value->mantissa = 0;
        value->exponent = 0;
        return CALC_OK;
    }
    calcEvaluateContext(evaluationContext,
                        app->gridCells[0][row][column], value, &error);
    if (error == CALC_OK)
        return CALC_OK;

    /* Keep the editor open and focus the exact cell which failed. */
    app->gridPanel = 0;
    app->gridRow = row;
    app->gridColumn = column;
    naturalCursorSetEnd(&app->gridCursor,
                        app->gridCellLengths[0][row][column]);
    app->runtime.error = error;
    app->runtime.result[0] = '\0';
    app->cursorVisible = 1;
    appSetStatus(app, calcErrorText(error));
    app->dirty |= APP_DIRTY_EXPRESSION | APP_DIRTY_RESULT |
                  APP_DIRTY_VIEWPORT;
    return error;
}

static u8 appSaveNamedGridRegister(AppState *app)
{
    union AppNamedGridValues {
        CalcNumber matrix[MODE_MATRIX_MAX_ROWS]
                         [MODE_MATRIX_MAX_COLUMNS];
        CalcNumber vector[MODE_VECTOR_MAX_DIMENSIONS];
    } values;
    CalcContext evaluationContext;
    char saved[16];
    ModeNamedRegister name;
    const char *label;
    u8 error = CALC_OK;
    u8 stored;
    u8 row;
    u8 column;

    if (!appNamedGridRegister(app, &name))
        return 0;
    memset(&values, 0, sizeof(values));
    evaluationContext = app->runtime.calc;
    if (app->mode == CALC_MODE_MATRIX) {
        label = modeMatrixRegisterLabel(name);
        for (row = 0; row < app->gridRows[0]; row++) {
            for (column = 0; column < app->gridColumns[0]; column++) {
                error = appEvaluateNamedGridCell(app, &evaluationContext,
                    row, column, &values.matrix[row][column]);
                if (error != CALC_OK)
                    return 0;
            }
        }
        stored = modeMatrixSetRegister(&app->runtime, name, values.matrix,
                                       app->gridRows[0],
                                       app->gridColumns[0]);
    } else {
        label = modeVectorRegisterLabel(name);
        for (column = 0; column < app->gridColumns[0]; column++) {
            error = appEvaluateNamedGridCell(app, &evaluationContext,
                0, column, &values.vector[column]);
            if (error != CALC_OK)
                return 0;
        }
        stored = modeVectorSetRegister(&app->runtime, name, values.vector,
                                       app->gridColumns[0]);
    }
    error = stored ? CALC_OK : CALC_ERR_SYNTAX;
    app->runtime.error = error;
    if (error != CALC_OK) {
        appSetStatus(app, calcErrorText(error));
        app->dirty |= APP_DIRTY_RESULT | APP_DIRTY_EXPRESSION;
        return 0;
    }
    app->runtime.calc = evaluationContext;
    app->runtime.result[0] = '\0';
    appCopyText(saved, sizeof(saved), label);
    appAppendText(saved, sizeof(saved), " SAVED");
    appSetStatus(app, saved);
    app->view = APP_VIEW_MODE_ACTION;
    app->cursorVisible = 0;
    app->dirty = APP_DIRTY_ALL;
    return 1;
}

static void appEvaluateGrid(AppState *app)
{
    char source[640];
    u8 error;

    if (!appBuildGridExpression(app, source, sizeof(source))) {
        appSetStatus(app, "COMPLETE ALL CELLS");
        return;
    }
    appCopyText(app->expression, sizeof(app->expression), source);
    app->expressionLength = (u8)strlen(app->expression);
    naturalCursorSetEnd(&app->cursor, app->expressionLength);
    error = modeEvaluate(&app->runtime, app->mode, source);
    appSetStatus(app, error == CALC_OK ? "DONE" : calcErrorText(error));
    app->dirty |= APP_DIRTY_RESULT;
}

static void appMoveGridCell(AppState *app, s8 horizontal, s8 vertical)
{
    u8 panel = app->gridPanel;
    u8 row = app->gridRow;
    u8 column = app->gridColumn;
    u8 rows;
    u8 columns;

    if (panel >= app->gridPanelCount)
        panel = 0;
    rows = app->gridRows[panel];
    columns = app->gridColumns[panel];
    if (row >= rows)
        row = 0;
    if (column >= columns)
        column = 0;

    if (horizontal < 0) {
        if (column != 0)
            column--;
        else if (row != 0) {
            row--;
            column = (u8)(columns - 1);
        } else {
            panel = (u8)((panel + app->gridPanelCount - 1) %
                         app->gridPanelCount);
            rows = app->gridRows[panel];
            columns = app->gridColumns[panel];
            row = (u8)(rows - 1);
            column = (u8)(columns - 1);
        }
    } else if (horizontal > 0) {
        column++;
        if (column >= columns) {
            column = 0;
            row++;
            if (row >= rows) {
                panel = (u8)((panel + 1) % app->gridPanelCount);
                rows = app->gridRows[panel];
                columns = app->gridColumns[panel];
                row = 0;
            }
        }
    } else if (vertical < 0) {
        row = (u8)((row + rows - 1) % rows);
    } else if (vertical > 0) {
        row = (u8)((row + 1) % rows);
    }
    if (column >= app->gridColumns[panel])
        column = (u8)(app->gridColumns[panel] - 1);
    app->gridPanel = panel;
    app->gridRow = row;
    app->gridColumn = column;
    naturalCursorSetEnd(&app->gridCursor,
        app->gridCellLengths[panel][row][column]);
    app->cursorVisible = 1;
    appUpdateGridStatus(app);
    app->dirty |= APP_DIRTY_EXPRESSION | APP_DIRTY_VIEWPORT;
}

static void appCycleGridDimension(AppState *app, u8 columns)
{
    u8 panel = app->gridPanel;
    u8 value;

    if (app->mode == CALC_MODE_VECTOR) {
        if (app->formAction == APP_FORM_VECTOR_CROSS)
            return;
        u8 vectorPanel = app->formAction == APP_FORM_VECTOR_SCALE ? 1 : 0;
        u8 size = app->gridColumns[vectorPanel] == 3 ? 2 : 3;
        app->gridColumns[vectorPanel] = size;
        if (app->gridPanelCount == 2 &&
            app->formAction != APP_FORM_VECTOR_SCALE)
            app->gridColumns[1] = size;
    } else if (app->mode == CALC_MODE_MATRIX) {
        if (columns) {
            value = (u8)(app->gridColumns[panel] % 4 + 1);
            app->gridColumns[panel] = value;
        } else {
            value = (u8)(app->gridRows[panel] % 4 + 1);
            app->gridRows[panel] = value;
        }
        if (app->formAction == APP_FORM_MATRIX_DET ||
            app->formAction == APP_FORM_MATRIX_INV) {
            value = columns ? app->gridColumns[0] : app->gridRows[0];
            app->gridRows[0] = value;
            app->gridColumns[0] = value;
        } else if (app->formAction == APP_FORM_MATRIX_ADD ||
                   app->formAction == APP_FORM_MATRIX_SUB) {
            app->gridRows[1] = app->gridRows[0];
            app->gridColumns[1] = app->gridColumns[0];
        } else if (app->formAction == APP_FORM_MATRIX_MUL) {
            if (panel == 0 && columns)
                app->gridRows[1] = app->gridColumns[0];
            else if (panel == 1 && !columns)
                app->gridColumns[0] = app->gridRows[1];
        }
    }
    if (app->gridRow >= app->gridRows[app->gridPanel])
        app->gridRow = (u8)(app->gridRows[app->gridPanel] - 1);
    if (app->gridColumn >= app->gridColumns[app->gridPanel])
        app->gridColumn = (u8)(app->gridColumns[app->gridPanel] - 1);
    naturalCursorSetEnd(&app->gridCursor,
        app->gridCellLengths[app->gridPanel][app->gridRow]
                            [app->gridColumn]);
    appUpdateGridStatus(app);
    app->dirty |= APP_DIRTY_VIEWPORT | APP_DIRTY_EXPRESSION;
}

static void appInsertGridSelected(AppState *app)
{
    const AppToken *token;
    char *cell;
    u8 *length;

    if (app->selectedToken == APP_KEYPAD_EXE) {
        appMoveGridCell(app, 1, 0);
        return;
    }
    token = appSharedToken(app->mode, app->tokenPage,
                           app->selectedToken);
    cell = app->gridCells[app->gridPanel][app->gridRow][app->gridColumn];
    length = &app->gridCellLengths[app->gridPanel][app->gridRow]
                                  [app->gridColumn];
    if (!appInsertBufferToken(cell, length, APP_GRID_CELL_CAPACITY,
                              &app->gridCursor, token)) {
        appSetStatus(app, "CELL FULL");
        return;
    }
    app->runtime.result[0] = '\0';
    app->runtime.error = CALC_OK;
    app->cursorVisible = 1;
    appUpdateGridStatus(app);
    app->dirty |= APP_DIRTY_EXPRESSION | APP_DIRTY_RESULT |
                  APP_DIRTY_VIEWPORT;
}

static void appHandleGridKeys(AppState *app, u16 pressed, u16 held)
{
    char *cell = app->gridCells[app->gridPanel][app->gridRow]
                               [app->gridColumn];
    u8 *length = &app->gridCellLengths[app->gridPanel][app->gridRow]
                                      [app->gridColumn];
    ModeNamedRegister registerName;
    u8 namedEditor = appNamedGridRegister(app, &registerName);

    if (appChord(pressed, held, APP_KEY_B, APP_KEY_LEFT)) {
        appChangePage(app, -1);
        return;
    }
    if (appChord(pressed, held, APP_KEY_B, APP_KEY_RIGHT)) {
        appChangePage(app, 1);
        return;
    }
    if (namedEditor &&
        appChord(pressed, held, APP_KEY_SELECT, APP_KEY_B)) {
        if (naturalCursorBackspace(cell, length, &app->gridCursor)) {
            app->runtime.result[0] = '\0';
            app->runtime.error = CALC_OK;
        }
        app->cursorVisible = 1;
        appUpdateGridStatus(app);
        app->dirty |= APP_DIRTY_EXPRESSION | APP_DIRTY_RESULT |
                      APP_DIRTY_VIEWPORT;
        return;
    }
    if ((held & APP_KEY_SELECT) != 0) {
        if ((pressed & APP_KEY_L) != 0 &&
            (app->mode == CALC_MODE_MATRIX ||
             app->mode == CALC_MODE_VECTOR))
            appCycleGridDimension(app, 1);
        else if ((pressed & APP_KEY_R) != 0 &&
                 (app->mode == CALC_MODE_MATRIX ||
                  app->mode == CALC_MODE_VECTOR))
            appCycleGridDimension(app, 0);
        else if (app->mode == CALC_MODE_INEQ &&
                 (pressed & (APP_KEY_UP | APP_KEY_DOWN)) != 0) {
            if ((pressed & APP_KEY_UP) != 0)
                app->gridRelation = (u8)((app->gridRelation + 3) % 4);
            else
                app->gridRelation = (u8)((app->gridRelation + 1) % 4);
            appUpdateGridStatus(app);
            app->dirty |= APP_DIRTY_VIEWPORT;
        } else if ((pressed & APP_KEY_LEFT) != 0)
            appMoveGridCell(app, -1, 0);
        else if ((pressed & APP_KEY_RIGHT) != 0)
            appMoveGridCell(app, 1, 0);
        else if ((pressed & APP_KEY_UP) != 0)
            appMoveGridCell(app, 0, -1);
        else if ((pressed & APP_KEY_DOWN) != 0)
            appMoveGridCell(app, 0, 1);
    } else {
        if ((pressed & APP_KEY_LEFT) != 0)
            appMoveKeypad(app, -1, 0);
        else if ((pressed & APP_KEY_RIGHT) != 0)
            appMoveKeypad(app, 1, 0);
        else if ((pressed & APP_KEY_UP) != 0)
            appMoveKeypad(app, 0, -1);
        else if ((pressed & APP_KEY_DOWN) != 0)
            appMoveKeypad(app, 0, 1);
        if ((pressed & APP_KEY_L) != 0) {
            naturalCursorMoveHorizontal(cell, *length, &app->gridCursor,
                                        -1);
            app->cursorVisible = 1;
            app->dirty |= APP_DIRTY_EXPRESSION;
        }
        if ((pressed & APP_KEY_R) != 0) {
            naturalCursorMoveHorizontal(cell, *length, &app->gridCursor,
                                        1);
            app->cursorVisible = 1;
            app->dirty |= APP_DIRTY_EXPRESSION;
        }
    }
    if ((pressed & APP_KEY_A) != 0)
        appInsertGridSelected(app);
    if ((pressed & APP_KEY_B) != 0) {
        if (namedEditor) {
            (void)appSaveNamedGridRegister(app);
            return;
        }
        if (!naturalCursorBackspace(cell, length, &app->gridCursor)) {
            app->view = APP_VIEW_MODE_ACTION;
            app->dirty = APP_DIRTY_ALL;
            return;
        }
        app->runtime.result[0] = '\0';
        app->runtime.error = CALC_OK;
        app->cursorVisible = 1;
        app->dirty |= APP_DIRTY_EXPRESSION | APP_DIRTY_RESULT |
                      APP_DIRTY_VIEWPORT;
    }
    if ((pressed & APP_KEY_START) != 0) {
        if (namedEditor)
            (void)appSaveNamedGridRegister(app);
        else
            appEvaluateGrid(app);
    }
}

static void appChangeBasePage(AppState *app, s8 direction)
{
    if (direction < 0)
        app->tokenPage = (u8)((app->tokenPage + APP_BASE_PAGES - 1) %
                              APP_BASE_PAGES);
    else
        app->tokenPage = (u8)((app->tokenPage + 1) % APP_BASE_PAGES);
    app->menuSelection = 0xff;
    appUpdateBaseStatus(app);
    app->dirty |= APP_DIRTY_KEYPAD | APP_DIRTY_HEADER;
}

static void appMoveBaseKeypad(AppState *app, s8 horizontal, s8 vertical)
{
    u8 old = app->selectedToken;
    u8 row = (u8)(old / APP_KEYPAD_COLUMNS);
    u8 column = (u8)(old % APP_KEYPAD_COLUMNS);
    u8 pageChanged = 0;

    if (old >= APP_TOKENS_PER_PAGE) {
        old = 0;
        row = 0;
        column = 0;
    }
    if (horizontal < 0) {
        if (column == 0) {
            column = APP_KEYPAD_COLUMNS - 1;
            app->tokenPage = (u8)((app->tokenPage + APP_BASE_PAGES - 1) %
                                  APP_BASE_PAGES);
            pageChanged = 1;
        } else
            column--;
    } else if (horizontal > 0) {
        if (column + 1 == APP_KEYPAD_COLUMNS) {
            column = 0;
            app->tokenPage = (u8)((app->tokenPage + 1) % APP_BASE_PAGES);
            pageChanged = 1;
        } else
            column++;
    } else if (vertical < 0) {
        if (row == 0) {
            row = APP_KEYPAD_ROWS - 1;
            app->tokenPage = (u8)((app->tokenPage + APP_BASE_PAGES - 1) %
                                  APP_BASE_PAGES);
            pageChanged = 1;
        } else
            row--;
    } else if (vertical > 0) {
        if (row + 1 == APP_KEYPAD_ROWS) {
            row = 0;
            app->tokenPage = (u8)((app->tokenPage + 1) % APP_BASE_PAGES);
            pageChanged = 1;
        } else
            row++;
    }
    app->selectedToken = (u8)(row * APP_KEYPAD_COLUMNS + column);
    if (pageChanged) {
        app->menuSelection = 0xff;
        appUpdateBaseStatus(app);
        app->dirty |= APP_DIRTY_HEADER;
    } else
        app->menuSelection = old;
    if (app->selectedToken != old || pageChanged)
        app->dirty |= APP_DIRTY_KEYPAD;
}

static u8 appBaseTokenEnabled(const AppState *app, u8 index)
{
    const AppToken *token;
    char value;
    u8 page = app->tokenPage < APP_BASE_PAGES ? app->tokenPage : 0;

    if (index >= APP_TOKENS_PER_PAGE || index < 4 ||
        index == APP_KEYPAD_EXE)
        return 1;
    token = &appBaseTokens[page][index];
    value = token->insert[0];
    if (token->insert[1] == '\0' &&
        ((value >= '0' && value <= '9') ||
         (value >= 'A' && value <= 'F')))
        return modeBaseDigitValid(modeGetBaseRadix(&app->runtime), value);
    return 1;
}

static void appEvaluateBase(AppState *app)
{
    u8 error;

    if (app->baseExpressionLength == 0) {
        appSetStatus(app, "ENTER A VALUE");
        return;
    }
    appCopyText(app->expression, sizeof(app->expression),
                app->baseExpression);
    app->expressionLength = app->baseExpressionLength;
    error = modeEvaluate(&app->runtime, CALC_MODE_BASEN,
                         app->baseExpression);
    appSetStatus(app, error == CALC_OK ? "DONE" : calcErrorText(error));
    app->dirty |= APP_DIRTY_RESULT;
}

static void appInsertBaseSelected(AppState *app)
{
    const AppToken *token;
    u8 page;

    if (app->selectedToken >= APP_TOKENS_PER_PAGE)
        app->selectedToken = 0;
    page = app->tokenPage < APP_BASE_PAGES ? app->tokenPage : 0;

    if (app->selectedToken == APP_KEYPAD_EXE) {
        appEvaluateBase(app);
        return;
    }
    if (app->selectedToken < 4) {
        static const BaseRadix radices[4] = {
            BASE_RADIX_DEC, BASE_RADIX_HEX,
            BASE_RADIX_BIN, BASE_RADIX_OCT
        };
        modeSetBaseRadix(&app->runtime, radices[app->selectedToken]);
        appUpdateBaseStatus(app);
        app->dirty |= APP_DIRTY_HEADER | APP_DIRTY_KEYPAD;
        return;
    }
    if (!appBaseTokenEnabled(app, app->selectedToken)) {
        appSetStatus(app, "DIGIT NOT IN RADIX");
        return;
    }
    token = &appBaseTokens[page][app->selectedToken];
    if (!appInsertBufferToken(app->baseExpression,
                              &app->baseExpressionLength,
                              APP_EXPRESSION_CAPACITY,
                              &app->baseCursor, token)) {
        appSetStatus(app, "EXPRESSION FULL");
        return;
    }
    app->runtime.result[0] = '\0';
    app->runtime.error = CALC_OK;
    app->cursorVisible = 1;
    appUpdateBaseStatus(app);
    app->dirty |= APP_DIRTY_EXPRESSION | APP_DIRTY_RESULT;
}

static void appHandleBaseKeys(AppState *app, u16 pressed, u16 held)
{
    if (appChord(pressed, held, APP_KEY_B, APP_KEY_LEFT)) {
        appChangeBasePage(app, -1);
        return;
    }
    if (appChord(pressed, held, APP_KEY_B, APP_KEY_RIGHT)) {
        appChangeBasePage(app, 1);
        return;
    }
    if ((pressed & APP_KEY_L) != 0) {
        naturalCursorMoveHorizontal(app->baseExpression,
                                    app->baseExpressionLength,
                                    &app->baseCursor, -1);
        app->cursorVisible = 1;
        app->dirty |= APP_DIRTY_EXPRESSION;
    }
    if ((pressed & APP_KEY_R) != 0) {
        naturalCursorMoveHorizontal(app->baseExpression,
                                    app->baseExpressionLength,
                                    &app->baseCursor, 1);
        app->cursorVisible = 1;
        app->dirty |= APP_DIRTY_EXPRESSION;
    }
    if ((pressed & APP_KEY_LEFT) != 0)
        appMoveBaseKeypad(app, -1, 0);
    else if ((pressed & APP_KEY_RIGHT) != 0)
        appMoveBaseKeypad(app, 1, 0);
    else if ((pressed & APP_KEY_UP) != 0)
        appMoveBaseKeypad(app, 0, -1);
    else if ((pressed & APP_KEY_DOWN) != 0)
        appMoveBaseKeypad(app, 0, 1);
    if ((pressed & APP_KEY_A) != 0)
        appInsertBaseSelected(app);
    if ((pressed & APP_KEY_B) != 0 &&
        naturalCursorBackspace(app->baseExpression,
                               &app->baseExpressionLength,
                               &app->baseCursor)) {
        app->runtime.result[0] = '\0';
        app->cursorVisible = 1;
        app->dirty |= APP_DIRTY_EXPRESSION | APP_DIRTY_RESULT;
    }
    if ((pressed & APP_KEY_START) != 0)
        appEvaluateBase(app);
}

static u8 appAppendChecked(char *destination, u16 capacity,
                           const char *source)
{
    u16 used = (u16)strlen(destination);
    u16 add = (u16)strlen(source);

    if (used + add + 1 > capacity)
        return 0;
    memcpy(destination + used, source, add + 1);
    return 1;
}

static u8 appStatColumnCount(const AppState *app)
{
    return modeGetStatModel(&app->runtime) == STAT_MODEL_1VAR ? 2 : 3;
}

static void appBeginStatData(AppState *app)
{
    StatModel model = (StatModel)app->actionSelection;
    StatModel oldModel = modeGetStatModel(&app->runtime);

    if (model >= STAT_MODEL_COUNT)
        model = STAT_MODEL_1VAR;
    if (model != oldModel) {
        memset(app->statCells, 0, sizeof(app->statCells));
        memset(app->statCellLengths, 0, sizeof(app->statCellLengths));
    }
    modeSetStatModel(&app->runtime, model);
    app->statRow = 0;
    app->statColumn = 0;
    app->statScroll = 0;
    app->tokenPage = 0;
    app->selectedToken = 0;
    naturalCursorSetEnd(&app->formCursor,
                        app->statCellLengths[0][0]);
    app->runtime.result[0] = '\0';
    app->runtime.error = CALC_OK;
    app->view = APP_VIEW_STAT_DATA;
    app->cursorVisible = 1;
    appUpdateStatStatus(app);
    app->dirty = APP_DIRTY_ALL;
}

static void appHandleStatTypeKeys(AppState *app, u16 pressed)
{
    u8 old = app->actionSelection;
    u8 row;
    u8 column;

    if (old >= STAT_MODEL_COUNT)
        old = 0;
    row = (u8)(old / 2);
    column = (u8)(old % 2);
    if ((pressed & APP_KEY_LEFT) != 0)
        column ^= 1;
    else if ((pressed & APP_KEY_RIGHT) != 0)
        column ^= 1;
    else if ((pressed & APP_KEY_UP) != 0)
        row = (u8)((row + 3) % 4);
    else if ((pressed & APP_KEY_DOWN) != 0)
        row = (u8)((row + 1) % 4);
    app->actionSelection = (u8)(row * 2 + column);
    if (app->actionSelection != old) {
        app->selectedToken = old;
        app->dirty |= APP_DIRTY_KEYPAD;
    }
    if ((pressed & (APP_KEY_A | APP_KEY_START)) != 0)
        appBeginStatData(app);
}

static void appMoveStatCell(AppState *app, s8 horizontal, s8 vertical)
{
    u8 columns = appStatColumnCount(app);

    if (horizontal < 0) {
        if (app->statColumn == 0) {
            app->statColumn = (u8)(columns - 1);
            app->statRow = (u8)((app->statRow + APP_STAT_MAX_ROWS - 1) %
                                APP_STAT_MAX_ROWS);
        } else
            app->statColumn--;
    } else if (horizontal > 0) {
        app->statColumn++;
        if (app->statColumn >= columns) {
            app->statColumn = 0;
            app->statRow = (u8)((app->statRow + 1) % APP_STAT_MAX_ROWS);
        }
    } else if (vertical < 0) {
        app->statRow = (u8)((app->statRow + APP_STAT_MAX_ROWS - 1) %
                            APP_STAT_MAX_ROWS);
    } else if (vertical > 0)
        app->statRow = (u8)((app->statRow + 1) % APP_STAT_MAX_ROWS);
    if (app->statRow < app->statScroll)
        app->statScroll = app->statRow;
    else if (app->statRow >= app->statScroll + 3)
        app->statScroll = (u8)(app->statRow - 2);
    naturalCursorSetEnd(&app->formCursor,
                        app->statCellLengths[app->statRow]
                                            [app->statColumn]);
    app->cursorVisible = 1;
    app->dirty |= APP_DIRTY_EXPRESSION | APP_DIRTY_VIEWPORT;
}

static void appEvaluateStat(AppState *app)
{
    const char *xValues[APP_STAT_MAX_ROWS];
    const char *yValues[APP_STAT_MAX_ROWS];
    const char *frequencies[APP_STAT_MAX_ROWS];
    StatModel model = modeGetStatModel(&app->runtime);
    u8 frequencyColumn = model == STAT_MODEL_1VAR ? 1 : 2;
    u8 count = 0;
    u8 row;
    u8 error;

    for (row = 0; row < APP_STAT_MAX_ROWS; row++) {
        if (app->statCellLengths[row][0] == 0)
            continue;
        xValues[count] = app->statCells[row][0];
        yValues[count] = model == STAT_MODEL_1VAR ? 0 :
                         app->statCells[row][1];
        frequencies[count] = app->statCellLengths[row][frequencyColumn] == 0 ?
                             0 : app->statCells[row][frequencyColumn];
        count++;
    }
    if (count == 0) {
        appSetStatus(app, "CHECK DATA TABLE");
        return;
    }
    error = modeStatEvaluateRows(&app->runtime, model, xValues,
                                 model == STAT_MODEL_1VAR ? 0 : yValues,
                                 frequencies, count);
    appSetStatus(app, error == CALC_OK ? "DONE" : calcErrorText(error));
    app->dirty |= APP_DIRTY_RESULT;
}

static void appInsertStatSelected(AppState *app)
{
    const AppToken *token;
    char *cell;
    u8 *length;

    if (app->selectedToken == APP_KEYPAD_EXE) {
        appMoveStatCell(app, 1, 0);
        return;
    }
    token = appSharedToken(CALC_MODE_STAT, app->tokenPage,
                           app->selectedToken);
    cell = app->statCells[app->statRow][app->statColumn];
    length = &app->statCellLengths[app->statRow][app->statColumn];
    if (!appInsertBufferToken(cell, length, APP_STAT_CELL_CAPACITY,
                              &app->formCursor, token)) {
        appSetStatus(app, "CELL FULL");
        return;
    }
    app->runtime.result[0] = '\0';
    app->runtime.error = CALC_OK;
    app->cursorVisible = 1;
    app->dirty |= APP_DIRTY_EXPRESSION | APP_DIRTY_RESULT |
                  APP_DIRTY_VIEWPORT;
}

static void appHandleStatDataKeys(AppState *app, u16 pressed, u16 held)
{
    char *cell = app->statCells[app->statRow][app->statColumn];
    u8 *length = &app->statCellLengths[app->statRow][app->statColumn];

    if (appChord(pressed, held, APP_KEY_B, APP_KEY_LEFT)) {
        appChangePage(app, -1);
        return;
    }
    if (appChord(pressed, held, APP_KEY_B, APP_KEY_RIGHT)) {
        appChangePage(app, 1);
        return;
    }
    if ((held & APP_KEY_SELECT) != 0) {
        if ((pressed & APP_KEY_LEFT) != 0)
            appMoveStatCell(app, -1, 0);
        else if ((pressed & APP_KEY_RIGHT) != 0)
            appMoveStatCell(app, 1, 0);
        else if ((pressed & APP_KEY_UP) != 0)
            appMoveStatCell(app, 0, -1);
        else if ((pressed & APP_KEY_DOWN) != 0)
            appMoveStatCell(app, 0, 1);
    } else {
        if ((pressed & APP_KEY_LEFT) != 0)
            appMoveKeypad(app, -1, 0);
        else if ((pressed & APP_KEY_RIGHT) != 0)
            appMoveKeypad(app, 1, 0);
        else if ((pressed & APP_KEY_UP) != 0)
            appMoveKeypad(app, 0, -1);
        else if ((pressed & APP_KEY_DOWN) != 0)
            appMoveKeypad(app, 0, 1);
    }
    if ((pressed & APP_KEY_L) != 0) {
        naturalCursorMoveHorizontal(cell, *length, &app->formCursor, -1);
        app->cursorVisible = 1;
        app->dirty |= APP_DIRTY_EXPRESSION;
    }
    if ((pressed & APP_KEY_R) != 0) {
        naturalCursorMoveHorizontal(cell, *length, &app->formCursor, 1);
        app->cursorVisible = 1;
        app->dirty |= APP_DIRTY_EXPRESSION;
    }
    if ((pressed & APP_KEY_A) != 0)
        appInsertStatSelected(app);
    if ((pressed & APP_KEY_B) != 0) {
        if (!naturalCursorBackspace(cell, length, &app->formCursor)) {
            app->actionSelection = (u8)modeGetStatModel(&app->runtime);
            app->view = APP_VIEW_STAT_TYPE;
            app->dirty = APP_DIRTY_ALL;
            return;
        }
        app->runtime.result[0] = '\0';
        app->cursorVisible = 1;
        app->dirty |= APP_DIRTY_EXPRESSION | APP_DIRTY_RESULT |
                      APP_DIRTY_VIEWPORT;
    }
    if ((pressed & APP_KEY_START) != 0)
        appEvaluateStat(app);
}

static void appChangeGraphPage(AppState *app, s8 direction)
{
    if (direction < 0)
        app->graphTokenPage = (u8)((app->graphTokenPage +
                                    APP_GRAPH_INPUT_PAGES - 1) %
                                   APP_GRAPH_INPUT_PAGES);
    else
        app->graphTokenPage = (u8)((app->graphTokenPage + 1) %
                                   APP_GRAPH_INPUT_PAGES);
    app->menuSelection = 0xff;
    appUpdateGraphInputStatus(app);
    app->dirty |= APP_DIRTY_KEYPAD | APP_DIRTY_HEADER;
}

static void appMoveGraphKeypad(AppState *app, s8 horizontal, s8 vertical)
{
    u8 old = app->graphSelectedToken;
    u8 row = (u8)(old / APP_GRAPH_INPUT_COLUMNS);
    u8 column = (u8)(old % APP_GRAPH_INPUT_COLUMNS);
    u8 pageChanged = 0;

    if (horizontal < 0) {
        if (column == 0) {
            column = APP_GRAPH_INPUT_COLUMNS - 1;
            app->graphTokenPage = (u8)((app->graphTokenPage +
                                        APP_GRAPH_INPUT_PAGES - 1) %
                                       APP_GRAPH_INPUT_PAGES);
            pageChanged = 1;
        } else
            column--;
    } else if (horizontal > 0) {
        if (column + 1 == APP_GRAPH_INPUT_COLUMNS) {
            column = 0;
            app->graphTokenPage = (u8)((app->graphTokenPage + 1) %
                                       APP_GRAPH_INPUT_PAGES);
            pageChanged = 1;
        } else
            column++;
    } else if (vertical < 0) {
        if (row == 0) {
            row = APP_GRAPH_INPUT_ROWS - 1;
            app->graphTokenPage = (u8)((app->graphTokenPage +
                                        APP_GRAPH_INPUT_PAGES - 1) %
                                       APP_GRAPH_INPUT_PAGES);
            pageChanged = 1;
        } else
            row--;
    } else if (vertical > 0) {
        if (row + 1 == APP_GRAPH_INPUT_ROWS) {
            row = 0;
            app->graphTokenPage = (u8)((app->graphTokenPage + 1) %
                                       APP_GRAPH_INPUT_PAGES);
            pageChanged = 1;
        } else
            row++;
    }
    app->graphSelectedToken = (u8)(row * APP_GRAPH_INPUT_COLUMNS + column);
    if (pageChanged) {
        app->menuSelection = 0xff;
        appUpdateGraphInputStatus(app);
        app->dirty |= APP_DIRTY_HEADER;
    } else
        app->menuSelection = old;
    if (app->graphSelectedToken != old || pageChanged)
        app->dirty |= APP_DIRTY_KEYPAD;
}

static void appInsertGraphSelected(AppState *app)
{
    const AppToken *token;
    u8 rewind;

    if (app->graphSelectedToken >= APP_GRAPH_INPUT_TOKENS)
        app->graphSelectedToken = 0;
    if (app->graphTokenPage >= APP_GRAPH_INPUT_PAGES)
        app->graphTokenPage = 0;

    if (app->graphSelectedToken == APP_GRAPH_INPUT_EXE) {
        appStartGraph(app);
        return;
    }
    token = &appGraphTokens[app->graphTokenPage]
                           [app->graphSelectedToken];
    if (!naturalCursorInsert(app->graphExpression,
                             &app->graphExpressionLength,
                             APP_EXPRESSION_CAPACITY, token->insert,
                             &app->graphCursor)) {
        appSetStatus(app, "EXPRESSION FULL");
        return;
    }
    rewind = token->rewind;
    while (rewind-- != 0)
        naturalCursorMoveHorizontal(app->graphExpression,
                                    app->graphExpressionLength,
                                    &app->graphCursor, -1);
    app->runtime.error = CALC_OK;
    app->cursorVisible = 1;
    appUpdateGraphInputStatus(app);
    app->dirty |= APP_DIRTY_EXPRESSION;
}

static void appHandleGraphInputKeys(AppState *app, u16 pressed, u16 held)
{
    if (appChord(pressed, held, APP_KEY_B, APP_KEY_LEFT)) {
        appChangeGraphPage(app, -1);
        return;
    }
    if (appChord(pressed, held, APP_KEY_B, APP_KEY_RIGHT)) {
        appChangeGraphPage(app, 1);
        return;
    }
    if (appChord(pressed, held, APP_KEY_SELECT, APP_KEY_UP)) {
        naturalCursorMoveVertical(app->graphExpression,
                                  app->graphExpressionLength,
                                  &app->graphCursor, -1);
        app->cursorVisible = 1;
        appUpdateGraphInputStatus(app);
        app->dirty |= APP_DIRTY_EXPRESSION;
        return;
    }
    if (appChord(pressed, held, APP_KEY_SELECT, APP_KEY_DOWN)) {
        naturalCursorMoveVertical(app->graphExpression,
                                  app->graphExpressionLength,
                                  &app->graphCursor, 1);
        app->cursorVisible = 1;
        appUpdateGraphInputStatus(app);
        app->dirty |= APP_DIRTY_EXPRESSION;
        return;
    }
    if ((pressed & APP_KEY_L) != 0) {
        naturalCursorMoveHorizontal(app->graphExpression,
                                    app->graphExpressionLength,
                                    &app->graphCursor, -1);
        app->cursorVisible = 1;
        appUpdateGraphInputStatus(app);
        app->dirty |= APP_DIRTY_EXPRESSION;
    }
    if ((pressed & APP_KEY_R) != 0) {
        naturalCursorMoveHorizontal(app->graphExpression,
                                    app->graphExpressionLength,
                                    &app->graphCursor, 1);
        app->cursorVisible = 1;
        appUpdateGraphInputStatus(app);
        app->dirty |= APP_DIRTY_EXPRESSION;
    }
    if ((held & APP_KEY_SELECT) == 0) {
        if ((pressed & APP_KEY_LEFT) != 0)
            appMoveGraphKeypad(app, -1, 0);
        else if ((pressed & APP_KEY_RIGHT) != 0)
            appMoveGraphKeypad(app, 1, 0);
        else if ((pressed & APP_KEY_UP) != 0)
            appMoveGraphKeypad(app, 0, -1);
        else if ((pressed & APP_KEY_DOWN) != 0)
            appMoveGraphKeypad(app, 0, 1);
    }
    if ((pressed & APP_KEY_A) != 0)
        appInsertGraphSelected(app);
    if ((pressed & APP_KEY_B) != 0) {
        if (naturalCursorBackspace(app->graphExpression,
                                   &app->graphExpressionLength,
                                   &app->graphCursor)) {
            app->runtime.error = CALC_OK;
            app->cursorVisible = 1;
            appUpdateGraphInputStatus(app);
            app->dirty |= APP_DIRTY_EXPRESSION;
        }
    }
    if ((pressed & APP_KEY_START) != 0)
        appStartGraph(app);
}

static s32 appGraphHalfWidth(const AppState *app)
{
    s32 halfWidth = (10L * CALC_ONE * 16L) >> app->zoom;
    return halfWidth < 8 ? 8 : halfWidth;
}

static void appUpdateTraceStatus(AppState *app)
{
    GraphFunction *function;
    GraphViewport viewport;
    GraphSampleState state;
    CalcNumber xNumber;
    CalcNumber yNumber;
    char xText[20];
    char yText[20];
    s32 parameter;
    s32 x;
    s32 y;

    if (!app->traceActive || app->graphFunctionCount == 0)
        return;
    if (app->selectedToken >= app->graphFunctionCount)
        app->selectedToken = 0;
    function = &app->graphFunctions[app->selectedToken];
    viewport = appViewport(app);
    if (function->row.kind == CALC_GRAPH_ROW_X ||
        (function->row.kind == CALC_GRAPH_ROW_INEQUALITY &&
         function->row.axisX)) {
        parameter = (s32)(viewport.yMin +
                    ((s64)(viewport.yMax - viewport.yMin) *
                     app->cursor.offset) / (GBA_SCREEN_WIDTH - 1));
    } else if (function->row.kind == CALC_GRAPH_ROW_POLAR ||
               function->row.kind == CALC_GRAPH_ROW_PARAM) {
        s32 turn = 1608;
        if (app->runtime.calc.angleMode == CALC_ANGLE_DEG)
            turn = 360 * CALC_ONE;
        else if (app->runtime.calc.angleMode == CALC_ANGLE_GRAD)
            turn = 400 * CALC_ONE;
        parameter = (s32)(((s64)turn * app->cursor.offset) /
                          (GBA_SCREEN_WIDTH - 1));
    } else {
        parameter = (s32)(viewport.xMin +
                    ((s64)(viewport.xMax - viewport.xMin) *
                     app->cursor.offset) / (GBA_SCREEN_WIDTH - 1));
    }
    state = graphEvaluatePoint(function, &app->runtime.calc, parameter,
                               &x, &y);
    if (state != GRAPH_SAMPLE_VALID) {
        appSetStatus(app, "TRACE DOMAIN");
        return;
    }
    xNumber = calcNumberFromFixed(x);
    yNumber = calcNumberFromFixed(y);
    calcFormatNumber(xNumber, xText, sizeof(xText));
    calcFormatNumber(yNumber, yText, sizeof(yText));
    appCopyText(app->status, sizeof(app->status), "X=");
    appAppendText(app->status, sizeof(app->status), xText);
    appAppendText(app->status, sizeof(app->status), " Y=");
    appAppendText(app->status, sizeof(app->status), yText);
    app->dirty |= APP_DIRTY_STATUS;
}

static void appToggleTrace(AppState *app)
{
    app->traceActive = (u8)!app->traceActive;
    if (app->traceActive) {
        app->cursor.offset = GBA_SCREEN_WIDTH / 2;
        app->selectedToken = 0;
        appUpdateTraceStatus(app);
    } else {
        appSetStatus(app, app->graph.complete ? "PLOT COMPLETE" :
                                                 "PLOTTING");
    }
    appRestartGraph(app);
}

static void appHandleGraphKeys(AppState *app, u16 pressed, u16 held)
{
    if ((pressed & APP_KEY_B) != 0) {
        app->view = APP_VIEW_GRAPH_INPUT;
        app->traceActive = 0;
        app->cursorVisible = 1;
        appUpdateGraphInputStatus(app);
        app->dirty = APP_DIRTY_ALL;
        return;
    }
    if ((pressed & APP_KEY_START) != 0) {
        appStartGraph(app);
        return;
    }
    if ((pressed & APP_KEY_A) != 0 ||
        appChord(pressed, held, APP_KEY_SELECT, APP_KEY_UP)) {
        appToggleTrace(app);
        return;
    }
    if (app->traceActive) {
        if ((pressed & (APP_KEY_LEFT | APP_KEY_L)) != 0 &&
            app->cursor.offset > 0)
            app->cursor.offset--;
        if ((pressed & (APP_KEY_RIGHT | APP_KEY_R)) != 0 &&
            app->cursor.offset < GBA_SCREEN_WIDTH - 1)
            app->cursor.offset++;
        if ((pressed & APP_KEY_UP) != 0 &&
            (held & APP_KEY_SELECT) == 0 &&
            app->graphFunctionCount != 0)
            app->selectedToken = (u8)((app->selectedToken +
                                  app->graphFunctionCount - 1) %
                                  app->graphFunctionCount);
        if ((pressed & APP_KEY_DOWN) != 0 && app->graphFunctionCount != 0)
            app->selectedToken = (u8)((app->selectedToken + 1) %
                                      app->graphFunctionCount);
        if ((pressed & (APP_KEY_LEFT | APP_KEY_RIGHT | APP_KEY_UP |
                        APP_KEY_DOWN | APP_KEY_L | APP_KEY_R)) != 0) {
            appUpdateTraceStatus(app);
            app->dirty |= APP_DIRTY_VIEWPORT;
            appRestartGraph(app);
        }
    } else {
        if ((pressed & APP_KEY_R) != 0 && app->zoom < 8) {
            app->zoom++;
            appRestartGraph(app);
            app->dirty |= APP_DIRTY_HEADER;
        }
        if ((pressed & APP_KEY_L) != 0 && app->zoom > 0) {
            app->zoom--;
            appRestartGraph(app);
            app->dirty |= APP_DIRTY_HEADER;
        }
        if ((pressed & (APP_KEY_LEFT | APP_KEY_RIGHT | APP_KEY_UP |
                        APP_KEY_DOWN)) != 0) {
            s32 halfWidth = appGraphHalfWidth(app);
            s32 stepX = halfWidth / 4;
            s32 stepY = (s32)(((s64)stepX * APP_GRAPH_HEIGHT) /
                              GBA_SCREEN_WIDTH);

            if ((pressed & APP_KEY_LEFT) != 0)
                app->cameraX = (s16)appClampS16((s32)app->cameraX -
                                                stepX);
            if ((pressed & APP_KEY_RIGHT) != 0)
                app->cameraX = (s16)appClampS16((s32)app->cameraX +
                                                stepX);
            if ((pressed & APP_KEY_UP) != 0)
                app->cameraY = (s16)appClampS16((s32)app->cameraY +
                                                stepY);
            if ((pressed & APP_KEY_DOWN) != 0)
                app->cameraY = (s16)appClampS16((s32)app->cameraY -
                                                stepY);
            appRestartGraph(app);
        }
    }
}

static void appSelectMode(AppState *app)
{
    app->mode = (CalcMode)app->menuSelection;
    app->runtime.result[0] = '\0';
    app->runtime.error = CALC_OK;
    app->selectedToken = 0;
    app->tokenPage = 0;
    app->actionSelection = 0;
    app->view = appModeEntryView(app->mode);
    if (app->view == APP_VIEW_GRAPH_INPUT) {
        app->cursorVisible = 1;
        naturalCursorSetEnd(&app->graphCursor,
                            app->graphExpressionLength);
        appUpdateGraphInputStatus(app);
    } else if (app->view == APP_VIEW_CALCULATOR) {
        naturalCursorSetEnd(&app->cursor, app->expressionLength);
        app->cursorVisible = 1;
        appUpdateEditStatus(app);
    } else if (app->view == APP_VIEW_STAT_TYPE) {
        app->actionSelection = (u8)modeGetStatModel(&app->runtime);
        appSetStatus(app, "SELECT STAT TYPE");
    } else if (app->view == APP_VIEW_BASEN) {
        naturalCursorSetEnd(&app->baseCursor,
                            app->baseExpressionLength);
        app->cursorVisible = 1;
        appUpdateBaseStatus(app);
    } else {
        appSetStatus(app, "SELECT OPERATION");
    }
    app->dirty = APP_DIRTY_ALL;
}

static void appRestoreMenuView(AppState *app)
{
    app->view = app->menuReturnView;
    app->selectedToken = app->menuReturnSelection;
    if (app->view == APP_VIEW_CALCULATOR) {
        if (app->selectedToken >= APP_TOKENS_PER_PAGE)
            app->selectedToken = 0;
    } else if (app->view == APP_VIEW_TABLE) {
        u8 visibleRows = 13;
        u8 maximum = app->runtime.table.rows > visibleRows ?
                     (u8)(app->runtime.table.rows - visibleRows) : 0;
        if (app->selectedToken > maximum)
            app->selectedToken = maximum;
    } else if (app->view == APP_VIEW_GRAPH) {
        if (app->graphFunctionCount == 0 ||
            app->selectedToken >= app->graphFunctionCount)
            app->selectedToken = 0;
    }
}

static void appHandleMenuKeys(AppState *app, u16 pressed)
{
    u8 old = app->menuSelection;
    u8 row = (u8)(old / 3);
    u8 column = (u8)(old % 3);

    if ((pressed & APP_KEY_B) != 0) {
        appRestoreMenuView(app);
        app->dirty = APP_DIRTY_ALL;
        return;
    }
    if ((pressed & APP_KEY_LEFT) != 0)
        column = (u8)((column + 2) % 3);
    else if ((pressed & APP_KEY_RIGHT) != 0)
        column = (u8)((column + 1) % 3);
    else if ((pressed & APP_KEY_UP) != 0)
        row = (u8)((row + 3) % 4);
    else if ((pressed & APP_KEY_DOWN) != 0)
        row = (u8)((row + 1) % 4);
    app->menuSelection = (u8)(row * 3 + column);
    if (app->menuSelection != old) {
        app->selectedToken = old;
        app->dirty |= APP_DIRTY_KEYPAD;
    }
    if ((pressed & (APP_KEY_A | APP_KEY_START)) != 0)
        appSelectMode(app);
}

static void appHandleTableKeys(AppState *app, u16 pressed)
{
    u8 visibleRows = 13;
    u8 maximum = app->runtime.table.rows > visibleRows ?
                 (u8)(app->runtime.table.rows - visibleRows) : 0;

    if ((pressed & APP_KEY_B) != 0) {
        app->selectedToken = 0;
        if (app->mode == CALC_MODE_TABLE && appCurrentAction(app) != 0) {
            app->view = APP_VIEW_MODE_FORM;
            app->formField = 0;
            naturalCursorSetEnd(&app->formCursor,
                                app->formLengths[0]);
            appUpdateFormStatus(app);
        } else {
            app->view = APP_VIEW_CALCULATOR;
            naturalCursorSetEnd(&app->cursor, app->expressionLength);
            appUpdateEditStatus(app);
        }
        app->dirty = APP_DIRTY_ALL;
        return;
    }
    if ((pressed & APP_KEY_START) != 0) {
        appEvaluate(app);
        return;
    }
    if ((pressed & APP_KEY_UP) != 0 && app->selectedToken > 0) {
        app->selectedToken--;
        app->dirty |= APP_DIRTY_VIEWPORT;
    }
    if ((pressed & APP_KEY_DOWN) != 0 && app->selectedToken < maximum) {
        app->selectedToken++;
        app->dirty |= APP_DIRTY_VIEWPORT;
    }
}

void appHandleKeys(AppState *app, u16 pressed, u16 held)
{
    if (app == 0)
        return;
    if (appChord(pressed, held, APP_KEY_SELECT, APP_KEY_START)) {
        if (app->view == APP_VIEW_MENU) {
            appRestoreMenuView(app);
        } else {
            app->menuReturnView = app->view;
            app->menuReturnSelection = app->selectedToken;
            app->view = APP_VIEW_MENU;
            app->menuSelection = (u8)app->mode;
        }
        app->dirty = APP_DIRTY_ALL;
        return;
    }
    switch (app->view) {
    case APP_VIEW_MENU:
        appHandleMenuKeys(app, pressed);
        break;
    case APP_VIEW_GRAPH:
        appHandleGraphKeys(app, pressed, held);
        break;
    case APP_VIEW_GRAPH_INPUT:
        appHandleGraphInputKeys(app, pressed, held);
        break;
    case APP_VIEW_TABLE:
        appHandleTableKeys(app, pressed);
        break;
    case APP_VIEW_MODE_ACTION:
        appHandleActionKeys(app, pressed);
        break;
    case APP_VIEW_MODE_FORM:
        appHandleFormKeys(app, pressed, held);
        break;
    case APP_VIEW_MODE_GRID:
        appHandleGridKeys(app, pressed, held);
        break;
    case APP_VIEW_STAT_TYPE:
        appHandleStatTypeKeys(app, pressed);
        break;
    case APP_VIEW_STAT_DATA:
        appHandleStatDataKeys(app, pressed, held);
        break;
    case APP_VIEW_BASEN:
        appHandleBaseKeys(app, pressed, held);
        break;
    case APP_VIEW_CALCULATOR:
    default:
        appHandleCalculatorKeys(app, pressed, held);
        break;
    }
}

void appTick(AppState *app)
{
    if (app == 0)
        return;
    app->frame++;
    if ((app->view == APP_VIEW_CALCULATOR ||
         app->view == APP_VIEW_GRAPH_INPUT ||
         app->view == APP_VIEW_MODE_FORM ||
         app->view == APP_VIEW_MODE_GRID ||
         app->view == APP_VIEW_STAT_DATA ||
         app->view == APP_VIEW_BASEN) && app->frame % 30 == 0) {
        app->cursorVisible = (u8)!app->cursorVisible;
        app->dirty |= app->view == APP_VIEW_STAT_DATA ||
                      app->view == APP_VIEW_MODE_GRID ?
                      APP_DIRTY_VIEWPORT : APP_DIRTY_EXPRESSION;
    }
}

static void appRenderHeader(AppState *app, GfxSurface *surface)
{
    const char *label = app->view == APP_VIEW_MENU ? "MODE" :
                        modeLabel(app->mode);

    gfxFillRect(surface, 0, APP_HEADER_Y, GBA_SCREEN_WIDTH,
                APP_HEADER_HEIGHT, APP_COLOR_PANEL_ALT);
    gfxText5x7(surface, 3, 3, label, APP_COLOR_TEXT);
    if (app->view == APP_VIEW_GRAPH) {
        char zoom[5] = {'Z', ':', '0', '\0', '\0'};
        zoom[2] = (char)('0' + app->zoom);
        gfxText5x7(surface, 211, 3, zoom, APP_COLOR_MUTED);
    } else if (app->view == APP_VIEW_GRAPH_INPUT) {
        const char *page = appGraphPageLabels[app->graphTokenPage];
        s16 x = (s16)(237 - strlen(page) * GFX_ASCII5X7_ADVANCE);
        gfxText5x7(surface, x, 3, page,
                   APP_COLOR_MUTED);
    } else if (app->view == APP_VIEW_BASEN) {
        const char *page = appBasePageLabels[
            app->tokenPage < APP_BASE_PAGES ? app->tokenPage : 0];
        s16 x = (s16)(237 - strlen(page) * GFX_ASCII5X7_ADVANCE);
        gfxText5x7(surface, x, 3, page, APP_COLOR_MUTED);
    } else if (app->view == APP_VIEW_CALCULATOR ||
               app->view == APP_VIEW_MODE_FORM ||
               app->view == APP_VIEW_MODE_GRID ||
               app->view == APP_VIEW_STAT_DATA) {
        const char *page = appModePageLabel(app->mode, app->tokenPage);
        s16 x = (s16)(237 - strlen(page) * GFX_ASCII5X7_ADVANCE);
        gfxText5x7(surface, x, 3, page, APP_COLOR_MUTED);
    } else if (app->view == APP_VIEW_MODE_ACTION ||
               app->view == APP_VIEW_STAT_TYPE) {
        gfxText5x7(surface, 198, 3, "SELECT", APP_COLOR_MUTED);
    }
}

static void appTrimSpan(const char *text, s16 *start, s16 *end)
{
    while (*start < *end && appIsSpace(text[*start]))
        (*start)++;
    while (*end > *start && appIsSpace(text[*end - 1]))
        (*end)--;
}

static u8 appSpanStarts(const char *text, s16 start, s16 end,
                        const char *prefix)
{
    while (*prefix != '\0') {
        if (start >= end || appLower(text[start]) != appLower(*prefix))
            return 0;
        start++;
        prefix++;
    }
    return 1;
}

static s16 appMatchingClose(const char *text, s16 open, s16 end)
{
    s16 depth = 1;
    s16 index;

    for (index = (s16)(open + 1); index < end; index++) {
        if (text[index] == '(')
            depth++;
        else if (text[index] == ')') {
            depth--;
            if (depth == 0)
                return index;
        }
    }
    return -1;
}

static u8 appUnaryAt(const char *text, s16 start, s16 position)
{
    s16 index = position;

    while (index > start && appIsSpace(text[index - 1]))
        index--;
    if (index == start)
        return 1;
    return (u8)(text[index - 1] == '(' || text[index - 1] == ';' ||
                text[index - 1] == ',' || text[index - 1] == '+' ||
                text[index - 1] == '-' || text[index - 1] == '*' ||
                text[index - 1] == '/' || text[index - 1] == '^' ||
                text[index - 1] == '=' || text[index - 1] == '<' ||
                text[index - 1] == '>');
}

static NaturalSplit appFindLowSplit(const char *text, s16 start, s16 end)
{
    NaturalSplit split = {-1, 0};
    s16 depth = 0;
    s16 index;

    for (index = (s16)(end - 1); index >= start; index--) {
        char value = text[index];
        if (value == ')')
            depth++;
        else if (value == '(')
            depth--;
        else if (depth == 0) {
            if ((value == '+' || value == '-') &&
                !appUnaryAt(text, start, index)) {
                split.position = index;
                split.width = 1;
                return split;
            }
            if (value == '=' || value == '<' || value == '>' ||
                value == '!') {
                split.position = index;
                split.width = (u8)((index + 1 < end &&
                              text[index + 1] == '=') ? 2 : 1);
                return split;
            }
        }
    }
    depth = 0;
    for (index = (s16)(end - 1); index >= start; index--) {
        if (text[index] == ')')
            depth++;
        else if (text[index] == '(')
            depth--;
        else if (depth == 0 && text[index] == '*') {
            split.position = index;
            split.width = 1;
            return split;
        }
    }
    return split;
}

static s16 appFindTopLevel(const char *text, s16 start, s16 end,
                           char wanted, u8 fromRight)
{
    s16 depth = 0;
    s16 found = -1;
    s16 index;

    for (index = start; index < end; index++) {
        if (text[index] == '(')
            depth++;
        else if (text[index] == ')')
            depth--;
        else if (depth == 0 && text[index] == wanted) {
            found = index;
            if (!fromRight)
                return found;
        }
    }
    return found;
}

static NaturalBox appNaturalMeasure(const char *text, s16 start, s16 end,
                                    u8 depth);

static NaturalBox appNaturalSequenceBox(NaturalBox left, NaturalBox right,
                                        u8 operatorWidth)
{
    NaturalBox box;
    box.width = (s16)(left.width + operatorWidth * 6 + right.width);
    box.above = left.above > right.above ? left.above : right.above;
    box.below = left.below > right.below ? left.below : right.below;
    return box;
}

static u8 appNaturalRootParts(const char *text, s16 start, s16 end,
                              s16 *contentStart, s16 *separator,
                              u8 *indexed)
{
    s16 open;
    s16 close;

    *separator = -1;
    *indexed = 0;
    if (appSpanStarts(text, start, end, "sqrt("))
        open = (s16)(start + 4);
    else if (appSpanStarts(text, start, end, "root(")) {
        open = (s16)(start + 4);
        *indexed = 1;
    } else if (appSpanStarts(text, start, end, "nroot(")) {
        open = (s16)(start + 5);
        *indexed = 1;
    } else
        return 0;
    close = appMatchingClose(text, open, end);
    if (close != end - 1)
        return 0;
    *contentStart = (s16)(open + 1);
    if (*indexed) {
        *separator = appFindTopLevel(text, *contentStart, close, ';', 0);
        if (*separator < 0)
            *separator = appFindTopLevel(text, *contentStart, close, ',', 0);
        if (*separator < 0)
            return 0;
    }
    return 1;
}

static NaturalBox appNaturalMeasure(const char *text, s16 start, s16 end,
                                    u8 depth)
{
    NaturalBox box;
    NaturalSplit split;
    s16 operatorPosition;
    s16 contentStart;
    s16 separator;
    u8 indexed;

    appTrimSpan(text, &start, &end);
    box.width = (s16)((end - start) * 6);
    box.above = 0;
    box.below = 9;
    if (start >= end) {
        box.width = 1;
        return box;
    }
    if (depth >= 10)
        return box;

    split = appFindLowSplit(text, start, end);
    if (split.position > start &&
        split.position + split.width < end) {
        NaturalBox left = appNaturalMeasure(text, start, split.position,
                                            (u8)(depth + 1));
        NaturalBox right = appNaturalMeasure(text,
                              (s16)(split.position + split.width), end,
                              (u8)(depth + 1));
        return appNaturalSequenceBox(left, right, split.width);
    }
    operatorPosition = appFindTopLevel(text, start, end, '/', 1);
    if (operatorPosition > start && operatorPosition + 1 < end) {
        NaturalBox numerator = appNaturalMeasure(text, start,
                                  operatorPosition, (u8)(depth + 1));
        NaturalBox denominator = appNaturalMeasure(text,
                                  (s16)(operatorPosition + 1), end,
                                  (u8)(depth + 1));
        s16 numeratorTotal = (s16)(numerator.above + numerator.below);
        s16 denominatorTotal = (s16)(denominator.above +
                                     denominator.below);
        box.width = (s16)((numerator.width > denominator.width ?
                          numerator.width : denominator.width) + 4);
        box.above = numeratorTotal > 3 ?
                    (s16)(numeratorTotal - 3) : 0;
        box.below = (s16)(6 + denominatorTotal);
        return box;
    }
    operatorPosition = appFindTopLevel(text, start, end, '^', 0);
    if (operatorPosition > start && operatorPosition + 1 < end) {
        NaturalBox base = appNaturalMeasure(text, start, operatorPosition,
                                            (u8)(depth + 1));
        NaturalBox exponent = appNaturalMeasure(text,
                              (s16)(operatorPosition + 1), end,
                              (u8)(depth + 1));
        box.width = (s16)(base.width + exponent.width);
        box.above = (s16)(7 + exponent.above);
        box.below = base.below;
        if (exponent.below > 7 && exponent.below - 7 > box.below)
            box.below = (s16)(exponent.below - 7);
        return box;
    }
    if (text[start] == '(' && appMatchingClose(text, start, end) == end - 1) {
        NaturalBox inside = appNaturalMeasure(text, (s16)(start + 1),
                                              (s16)(end - 1),
                                              (u8)(depth + 1));
        inside.width = (s16)(inside.width + 12);
        return inside;
    }
    if (appNaturalRootParts(text, start, end, &contentStart, &separator,
                            &indexed)) {
        s16 close = (s16)(end - 1);
        if (!indexed) {
            NaturalBox radicand = appNaturalMeasure(text, contentStart,
                                  close, (u8)(depth + 1));
            box.width = (s16)(radicand.width + 9);
            box.above = radicand.above > 3 ? radicand.above : 3;
            box.below = radicand.below;
        } else {
            NaturalBox index = appNaturalMeasure(text, contentStart,
                               separator, (u8)(depth + 1));
            NaturalBox radicand = appNaturalMeasure(text,
                                  (s16)(separator + 1), close,
                                  (u8)(depth + 1));
            s16 lead = index.width > 8 ? index.width : 8;
            box.width = (s16)(lead + 7 + radicand.width);
            box.above = (s16)(index.above + 6);
            if (radicand.above > box.above)
                box.above = radicand.above;
            if (box.above < 3)
                box.above = 3;
            box.below = radicand.below;
        }
    }
    return box;
}

static void appPainterCursor(NaturalPainter *painter, s16 x, s16 y)
{
    if (!painter->cursorFound) {
        painter->cursorFound = 1;
        painter->cursorX = x;
        painter->cursorY = y;
    }
}

static void appDrawLinearSpan(NaturalPainter *painter, s16 start, s16 end,
                              s16 x, s16 anchorY)
{
    s16 index;

    for (index = start; index < end; index++) {
        if (!painter->cursorFound && painter->cursorOffset == index)
            appPainterCursor(painter, x, (s16)(anchorY + 1));
        gfxChar5x7(painter->surface, x, anchorY,
                   painter->text[index], painter->color);
        x = (s16)(x + 6);
    }
    if (!painter->cursorFound && painter->cursorOffset == end)
        appPainterCursor(painter, x, (s16)(anchorY + 1));
}

static void appNaturalDraw(NaturalPainter *painter, s16 start, s16 end,
                           s16 x, s16 anchorY, u8 depth)
{
    NaturalSplit split;
    s16 operatorPosition;
    s16 contentStart;
    s16 separator;
    u8 indexed;

    appTrimSpan(painter->text, &start, &end);
    if (start >= end || depth >= 10) {
        appDrawLinearSpan(painter, start, end, x, anchorY);
        return;
    }
    split = appFindLowSplit(painter->text, start, end);
    if (split.position > start &&
        split.position + split.width < end) {
        NaturalBox left = appNaturalMeasure(painter->text, start,
                                            split.position,
                                            (u8)(depth + 1));
        appNaturalDraw(painter, start, split.position, x, anchorY,
                       (u8)(depth + 1));
        appDrawLinearSpan(painter, split.position,
                          (s16)(split.position + split.width),
                          (s16)(x + left.width), anchorY);
        appNaturalDraw(painter, (s16)(split.position + split.width), end,
                       (s16)(x + left.width + split.width * 6), anchorY,
                       (u8)(depth + 1));
        return;
    }
    operatorPosition = appFindTopLevel(painter->text, start, end, '/', 1);
    if (operatorPosition > start && operatorPosition + 1 < end) {
        NaturalBox numerator = appNaturalMeasure(painter->text, start,
                                  operatorPosition, (u8)(depth + 1));
        NaturalBox denominator = appNaturalMeasure(painter->text,
                                  (s16)(operatorPosition + 1), end,
                                  (u8)(depth + 1));
        s16 width = (s16)((numerator.width > denominator.width ?
                           numerator.width : denominator.width) + 4);
        s16 barY = (s16)(anchorY + 4);
        s16 numeratorAnchor = (s16)(barY - numerator.below - 1);
        s16 denominatorAnchor = (s16)(barY + 3 + denominator.above);
        appNaturalDraw(painter, start, operatorPosition,
                       (s16)(x + (width - numerator.width) / 2),
                       numeratorAnchor, (u8)(depth + 1));
        gfxLine(painter->surface, x, barY, (s16)(x + width - 1), barY,
                painter->color);
        appNaturalDraw(painter, (s16)(operatorPosition + 1), end,
                       (s16)(x + (width - denominator.width) / 2),
                       denominatorAnchor, (u8)(depth + 1));
        return;
    }
    operatorPosition = appFindTopLevel(painter->text, start, end, '^', 0);
    if (operatorPosition > start && operatorPosition + 1 < end) {
        NaturalBox base = appNaturalMeasure(painter->text, start,
                                            operatorPosition,
                                            (u8)(depth + 1));
        appNaturalDraw(painter, start, operatorPosition, x, anchorY,
                       (u8)(depth + 1));
        appNaturalDraw(painter, (s16)(operatorPosition + 1), end,
                       (s16)(x + base.width), (s16)(anchorY - 7),
                       (u8)(depth + 1));
        return;
    }
    if (painter->text[start] == '(' &&
        appMatchingClose(painter->text, start, end) == end - 1) {
        NaturalBox inside = appNaturalMeasure(painter->text,
                                              (s16)(start + 1),
                                              (s16)(end - 1),
                                              (u8)(depth + 1));
        appDrawLinearSpan(painter, start, (s16)(start + 1), x, anchorY);
        appNaturalDraw(painter, (s16)(start + 1), (s16)(end - 1),
                       (s16)(x + 6), anchorY, (u8)(depth + 1));
        appDrawLinearSpan(painter, (s16)(end - 1), end,
                          (s16)(x + 6 + inside.width), anchorY);
        return;
    }
    if (appNaturalRootParts(painter->text, start, end, &contentStart,
                            &separator, &indexed)) {
        s16 close = (s16)(end - 1);
        s16 lead = 0;
        NaturalBox radicand;

        if (indexed) {
            NaturalBox index = appNaturalMeasure(painter->text,
                               contentStart, separator,
                               (u8)(depth + 1));
            lead = index.width > 8 ? index.width : 8;
            appNaturalDraw(painter, contentStart, separator, x,
                           (s16)(anchorY - 6), (u8)(depth + 1));
            radicand = appNaturalMeasure(painter->text,
                                        (s16)(separator + 1), close,
                                        (u8)(depth + 1));
        } else {
            lead = 2;
            radicand = appNaturalMeasure(painter->text, contentStart,
                                        close, (u8)(depth + 1));
        }
        if (!painter->cursorFound && painter->cursorOffset < contentStart &&
            painter->cursorOffset >= start)
            appPainterCursor(painter, x, (s16)(anchorY + 1));
        gfxLine(painter->surface, (s16)(x + lead), (s16)(anchorY + 4),
                (s16)(x + lead + 2), (s16)(anchorY + 7), painter->color);
        gfxLine(painter->surface, (s16)(x + lead + 2),
                (s16)(anchorY + 7), (s16)(x + lead + 5),
                (s16)(anchorY - 2), painter->color);
        gfxLine(painter->surface, (s16)(x + lead + 5),
                (s16)(anchorY - 2),
                (s16)(x + lead + 7 + radicand.width),
                (s16)(anchorY - 2), painter->color);
        appNaturalDraw(painter,
                       indexed ? (s16)(separator + 1) : contentStart,
                       close, (s16)(x + lead + 7), anchorY,
                       (u8)(depth + 1));
        return;
    }
    appDrawLinearSpan(painter, start, end, x, anchorY);
}

static void appRenderNaturalExpression(GfxSurface *surface,
                                       const char *expression, u8 length,
                                       const NaturalCursor *cursor,
                                       u8 cursorVisible, s16 regionY,
                                       s16 regionHeight)
{
    NaturalBox box;
    NaturalPainter painter;
    s16 x = 4;
    s16 top;
    s16 anchor;

    gfxFillRect(surface, 0, regionY, GBA_SCREEN_WIDTH, regionHeight,
                APP_COLOR_BACKGROUND);
    box = appNaturalMeasure(expression, 0, length, 0);
    if (box.width > GBA_SCREEN_WIDTH - 8) {
        s16 approximate = (s16)(cursor->offset * 6);
        s16 maximumScroll = (s16)(box.width - (GBA_SCREEN_WIDTH - 8));
        s16 scroll = (s16)(approximate - (GBA_SCREEN_WIDTH / 2));
        if (scroll < 0)
            scroll = 0;
        if (scroll > maximumScroll)
            scroll = maximumScroll;
        x = (s16)(x - scroll);
    }
    top = (s16)(regionY +
          (regionHeight - box.above - box.below) / 2);
    if (top < regionY)
        top = regionY;
    anchor = (s16)(top + box.above);
    memset(&painter, 0, sizeof(painter));
    painter.surface = surface;
    painter.text = expression;
    painter.cursorOffset = cursor->offset;
    painter.color = APP_COLOR_TEXT;
    appNaturalDraw(&painter, 0, length, x, anchor, 0);
    if (!painter.cursorFound)
        appPainterCursor(&painter, (s16)(x + box.width),
                         (s16)(anchor + 1));
    if (cursorVisible)
        gfxLine(surface, painter.cursorX, painter.cursorY,
                painter.cursorX, (s16)(painter.cursorY + 6),
                APP_COLOR_CURSOR);
}

static void appRenderExpression(AppState *app, GfxSurface *surface)
{
    appRenderNaturalExpression(surface, app->expression,
                               app->expressionLength, &app->cursor,
                               app->cursorVisible, APP_EXPRESSION_Y,
                               APP_EXPRESSION_HEIGHT);
}

static void appRenderResult(AppState *app, GfxSurface *surface)
{
    u16 color = app->runtime.error == CALC_OK ? APP_COLOR_ACCENT :
                                                    APP_COLOR_ERROR;
    gfxFillRect(surface, 0, APP_RESULT_Y, GBA_SCREEN_WIDTH,
                APP_RESULT_HEIGHT, APP_COLOR_PANEL);
    gfxText5x7(surface, 3, APP_RESULT_Y + 2, app->runtime.result, color);
}

static void appRenderStatus(AppState *app, GfxSurface *surface)
{
    gfxFillRect(surface, 0, APP_STATUS_Y, GBA_SCREEN_WIDTH,
                APP_STATUS_HEIGHT, APP_COLOR_BACKGROUND);
    gfxText5x7(surface, 3, APP_STATUS_Y + 1, app->status,
               APP_COLOR_MUTED);
}

static void appRenderSharedKeypadCellAt(AppState *app, GfxSurface *surface,
                                        u8 index, s16 keypadY,
                                        s16 keypadHeight)
{
    s16 column = (s16)(index % APP_KEYPAD_COLUMNS);
    s16 row = (s16)(index / APP_KEYPAD_COLUMNS);
    s16 x = (s16)(column * (GBA_SCREEN_WIDTH / APP_KEYPAD_COLUMNS));
    s16 y = (s16)(keypadY +
                  (row * keypadHeight) / APP_KEYPAD_ROWS);
    s16 width = GBA_SCREEN_WIDTH / APP_KEYPAD_COLUMNS;
    s16 bottom = (s16)(keypadY +
                       ((row + 1) * keypadHeight) /
                       APP_KEYPAD_ROWS);
    s16 height = (s16)(bottom - y);
    u8 selected = index == app->selectedToken;
    u8 execute = index == APP_KEYPAD_EXE;
    u16 fill = selected ? APP_COLOR_ACCENT :
               (execute ? APP_COLOR_CURSOR : APP_COLOR_PANEL_ALT);
    u16 text = selected || execute ? APP_COLOR_BACKGROUND : APP_COLOR_TEXT;
    const char *label = appSharedToken(app->mode, app->tokenPage,
                                       index)->label;
    s16 labelWidth = (s16)(strlen(label) * GFX_ASCII5X7_ADVANCE);
    s16 textX = (s16)(x + (width - labelWidth) / 2);

    gfxFillRect(surface, x, y, width, height, fill);
    gfxRect(surface, x, y, width, height, APP_COLOR_BACKGROUND);
    gfxText5x7(surface, textX,
               (s16)(y + (height - GFX_ASCII5X7_HEIGHT) / 2),
               label, text);
}

static void appRenderKeypadCell(AppState *app, GfxSurface *surface,
                                u8 index)
{
    appRenderSharedKeypadCellAt(app, surface, index, APP_KEYPAD_Y,
                                APP_KEYPAD_HEIGHT);
}

static void appRenderKeypad(AppState *app, GfxSurface *surface, u8 full)
{
    u8 index;

    if (full || app->menuSelection == 0xff) {
        for (index = 0; index < APP_TOKENS_PER_PAGE; index++)
            appRenderKeypadCell(app, surface, index);
    } else {
        if (app->menuSelection < APP_TOKENS_PER_PAGE)
            appRenderKeypadCell(app, surface, app->menuSelection);
        appRenderKeypadCell(app, surface, app->selectedToken);
    }
}

static void appRenderSharedKeypadAt(AppState *app, GfxSurface *surface,
                                    u8 full, s16 keypadY,
                                    s16 keypadHeight)
{
    u8 index;

    if (full || app->menuSelection == 0xff) {
        for (index = 0; index < APP_TOKENS_PER_PAGE; index++)
            appRenderSharedKeypadCellAt(app, surface, index, keypadY,
                                        keypadHeight);
    } else {
        if (app->menuSelection < APP_TOKENS_PER_PAGE)
            appRenderSharedKeypadCellAt(app, surface,
                                        app->menuSelection, keypadY,
                                        keypadHeight);
        appRenderSharedKeypadCellAt(app, surface, app->selectedToken,
                                    keypadY, keypadHeight);
    }
}

static void appDrawClippedText(GfxSurface *surface, s16 x, s16 y,
                               s16 width, const char *source, u16 color)
{
    char text[40];
    u8 limit;
    u8 index = 0;

    if (width <= 0)
        return;
    limit = (u8)(width / GFX_ASCII5X7_ADVANCE);
    if (limit >= sizeof(text))
        limit = (u8)(sizeof(text) - 1);
    while (index < limit && source[index] != '\0') {
        text[index] = source[index];
        index++;
    }
    text[index] = '\0';
    gfxText5x7(surface, x, y, text, color);
}

static void appRenderActionCell(AppState *app, GfxSurface *surface,
                                u8 index, const char *label)
{
    s16 column = (s16)(index % APP_ACTION_COLUMNS);
    s16 row = (s16)(index / APP_ACTION_COLUMNS);
    s16 x = (s16)(column * (GBA_SCREEN_WIDTH / APP_ACTION_COLUMNS));
    s16 y = (s16)(APP_ACTION_TOP + row * APP_ACTION_CELL_HEIGHT);
    s16 width = GBA_SCREEN_WIDTH / APP_ACTION_COLUMNS;
    u8 selected = index == app->actionSelection;
    u16 fill = selected ? APP_COLOR_ACCENT : APP_COLOR_PANEL;
    u16 color = selected ? APP_COLOR_BACKGROUND : APP_COLOR_TEXT;

    gfxFillRect(surface, (s16)(x + 2), y, (s16)(width - 4),
                APP_ACTION_CELL_HEIGHT - 3, fill);
    gfxRect(surface, (s16)(x + 2), y, (s16)(width - 4),
            APP_ACTION_CELL_HEIGHT - 3, APP_COLOR_PANEL_ALT);
    appDrawClippedText(surface, (s16)(x + 5), (s16)(y + 8),
                       (s16)(width - 10), label, color);
}

static void appRenderModeActions(AppState *app, GfxSurface *surface,
                                 u8 full, u32 dirty)
{
    const AppActionSpec *specs;
    u8 count;
    u8 index;

    specs = appActionSpecs(app->mode, &count);
    if (full || (dirty & APP_DIRTY_HEADER) != 0)
        appRenderHeader(app, surface);
    if (!full && (dirty & APP_DIRTY_KEYPAD) == 0)
        return;
    gfxFillRect(surface, 0, APP_EXPRESSION_Y, GBA_SCREEN_WIDTH,
                GBA_SCREEN_HEIGHT - APP_EXPRESSION_Y,
                APP_COLOR_BACKGROUND);
    for (index = 0; index < count; index++)
        appRenderActionCell(app, surface, index, specs[index].label);
    gfxText5x7(surface, 3, 149, "A/START:OPEN  SEL+START:MODE",
               APP_COLOR_MUTED);
}

static void appRenderStatTypes(AppState *app, GfxSurface *surface,
                               u8 full, u32 dirty)
{
    u8 index;

    if (full || (dirty & APP_DIRTY_HEADER) != 0)
        appRenderHeader(app, surface);
    if (!full && (dirty & APP_DIRTY_KEYPAD) == 0)
        return;
    gfxFillRect(surface, 0, APP_EXPRESSION_Y, GBA_SCREEN_WIDTH,
                GBA_SCREEN_HEIGHT - APP_EXPRESSION_Y,
                APP_COLOR_BACKGROUND);
    for (index = 0; index < STAT_MODEL_COUNT; index++)
        appRenderActionCell(app, surface, index,
                            modeStatModelLabel((StatModel)index));
    gfxText5x7(surface, 3, 149, "A/START:DATA  X,FREQ OR X,Y,FREQ",
               APP_COLOR_MUTED);
}

static void appRenderFormFields(AppState *app, GfxSurface *surface)
{
    const AppActionSpec *spec = appCurrentAction(app);
    u8 visible = 5;
    u8 start = app->formField >= visible ?
               (u8)(app->formField - visible + 1) : 0;
    u8 row;

    gfxFillRect(surface, 0, APP_FORM_Y, GBA_SCREEN_WIDTH,
                APP_FORM_RESULT_Y - APP_FORM_Y, APP_COLOR_BACKGROUND);
    if (spec == 0)
        return;
    appDrawClippedText(surface, 3, 16, 234, spec->label,
                       APP_COLOR_ACCENT);
    for (row = 0; row < visible && start + row < spec->fieldCount; row++) {
        u8 field = (u8)(start + row);
        s16 y = (s16)(29 + row * 9);
        u8 selected = field == app->formField;
        u16 fill = selected ? APP_COLOR_PANEL_ALT : APP_COLOR_BACKGROUND;
        s16 cursorX;

        gfxFillRect(surface, 0, y, GBA_SCREEN_WIDTH, 9, fill);
        appDrawClippedText(surface, 3, (s16)(y + 1), 54,
                           spec->field[field], APP_COLOR_MUTED);
        appDrawClippedText(surface, 60, (s16)(y + 1), 177,
                           app->formFields[field], APP_COLOR_TEXT);
        if (!selected || !app->cursorVisible)
            continue;
        cursorX = (s16)(60 + app->formCursor.offset *
                       GFX_ASCII5X7_ADVANCE);
        if (cursorX > 237)
            cursorX = 237;
        gfxLine(surface, cursorX, (s16)(y + 1), cursorX,
                (s16)(y + 7), APP_COLOR_CURSOR);
    }
}

static void appRenderFormResult(AppState *app, GfxSurface *surface)
{
    u16 color = app->runtime.error == CALC_OK ? APP_COLOR_ACCENT :
                                                APP_COLOR_ERROR;

    gfxFillRect(surface, 0, APP_FORM_RESULT_Y, GBA_SCREEN_WIDTH,
                APP_FORM_RESULT_HEIGHT, APP_COLOR_PANEL);
    appDrawClippedText(surface, 3, APP_FORM_RESULT_Y + 1, 234,
                       app->runtime.result, color);
    appDrawClippedText(surface, 3, APP_FORM_RESULT_Y + 9, 234,
                       app->status, APP_COLOR_MUTED);
}

static void appRenderModeForm(AppState *app, GfxSurface *surface,
                              u8 full, u32 dirty)
{
    if (full || (dirty & APP_DIRTY_HEADER) != 0)
        appRenderHeader(app, surface);
    if (full || (dirty & APP_DIRTY_EXPRESSION) != 0)
        appRenderFormFields(app, surface);
    if (full || (dirty & (APP_DIRTY_RESULT | APP_DIRTY_STATUS)) != 0)
        appRenderFormResult(app, surface);
    if (full || (dirty & APP_DIRTY_KEYPAD) != 0)
        appRenderSharedKeypadAt(app, surface, full, APP_FORM_KEYPAD_Y,
                                APP_FORM_KEYPAD_HEIGHT);
}

static u8 appGridUsesTextbookBrackets(const AppState *app, u8 panel)
{
    if (app->mode == CALC_MODE_MATRIX)
        return 1;
    if (app->mode != CALC_MODE_VECTOR)
        return 0;

    /* SCALE owns a scalar K in panel zero.  Only its VctA panel is a vector. */
    return app->formAction != APP_FORM_VECTOR_SCALE || panel != 0;
}

static void appDrawLargeSquareBrackets(GfxSurface *surface, s16 top,
                                       s16 bottom)
{
    s16 height = (s16)(bottom - top + 1);
    s16 rightCap = (s16)(APP_GRID_BRACKET_RIGHT -
                         APP_GRID_BRACKET_CAP + 1);

    gfxFillRect(surface, APP_GRID_BRACKET_LEFT, top,
                APP_GRID_BRACKET_CAP, APP_GRID_BRACKET_STROKE,
                APP_COLOR_TEXT);
    gfxFillRect(surface, APP_GRID_BRACKET_LEFT, top,
                APP_GRID_BRACKET_STROKE, height, APP_COLOR_TEXT);
    gfxFillRect(surface, APP_GRID_BRACKET_LEFT,
                (s16)(bottom - APP_GRID_BRACKET_STROKE + 1),
                APP_GRID_BRACKET_CAP, APP_GRID_BRACKET_STROKE,
                APP_COLOR_TEXT);

    gfxFillRect(surface, rightCap, top, APP_GRID_BRACKET_CAP,
                APP_GRID_BRACKET_STROKE, APP_COLOR_TEXT);
    gfxFillRect(surface,
                (s16)(APP_GRID_BRACKET_RIGHT -
                      APP_GRID_BRACKET_STROKE + 1),
                top, APP_GRID_BRACKET_STROKE, height, APP_COLOR_TEXT);
    gfxFillRect(surface, rightCap,
                (s16)(bottom - APP_GRID_BRACKET_STROKE + 1),
                APP_GRID_BRACKET_CAP, APP_GRID_BRACKET_STROKE,
                APP_COLOR_TEXT);
}

static void appRenderTextbookGridCells(AppState *app, GfxSurface *surface,
                                       u8 panel, u8 rows, u8 columns)
{
    s16 naturalHeight = (s16)(rows * APP_GRID_TEXTBOOK_ROW_HEIGHT);
    s16 blockHeight = naturalHeight < APP_GRID_TEXTBOOK_MIN_HEIGHT ?
                      APP_GRID_TEXTBOOK_MIN_HEIGHT : naturalHeight;
    s16 blockTop = (s16)(APP_GRID_TABLE_Y +
                         (APP_GRID_TABLE_HEIGHT - blockHeight) / 2);
    s16 blockBottom = (s16)(blockTop + blockHeight - 1);
    s16 contentTop = (s16)(blockTop + (blockHeight - naturalHeight) / 2);
    s16 contentWidth = (s16)(APP_GRID_BRACKET_CONTENT_RIGHT -
                             APP_GRID_BRACKET_CONTENT_LEFT);
    u8 row;
    u8 column;

    appDrawLargeSquareBrackets(surface, blockTop, blockBottom);
    for (row = 0; row < rows; row++) {
        s16 rowTop = (s16)(contentTop +
                           row * APP_GRID_TEXTBOOK_ROW_HEIGHT);
        s16 textY = (s16)(rowTop +
                          (APP_GRID_TEXTBOOK_ROW_HEIGHT -
                           GFX_ASCII5X7_HEIGHT) / 2);

        for (column = 0; column < columns; column++) {
            const char *source = app->gridCells[panel][row][column];
            u8 selected = row == app->gridRow &&
                          column == app->gridColumn;
            s16 cellLeft = (s16)(APP_GRID_BRACKET_CONTENT_LEFT +
                                 (column * contentWidth) / columns);
            s16 cellRight = (s16)(APP_GRID_BRACKET_CONTENT_LEFT +
                                  ((column + 1) * contentWidth) /
                                  columns);
            s16 cellWidth = (s16)(cellRight - cellLeft);
            u8 maxCharacters = cellWidth > 6 ?
                               (u8)((cellWidth - 6) /
                                    GFX_ASCII5X7_ADVANCE) : 1;
            u8 sourceLength = app->gridCellLengths[panel][row][column];
            u8 viewStart = 0;
            u8 viewLength;
            char visible[APP_GRID_CELL_CAPACITY + 1];
            s16 textWidth;
            s16 textX;
            u8 index;

            if (maxCharacters > APP_GRID_CELL_CAPACITY)
                maxCharacters = APP_GRID_CELL_CAPACITY;
            if (selected)
                gfxFillRect(surface, (s16)(cellLeft + 1), rowTop,
                            (s16)(cellWidth - 2),
                            APP_GRID_TEXTBOOK_ROW_HEIGHT,
                            APP_COLOR_PANEL_ALT);

            if (sourceLength == 0) {
                visible[0] = '0';
                visible[1] = '\0';
                viewLength = 1;
            } else {
                if (sourceLength > maxCharacters) {
                    if (selected && app->gridCursor.offset >=
                                    maxCharacters) {
                        viewStart = (u8)(app->gridCursor.offset -
                                        maxCharacters + 1);
                    } else {
                        viewStart = (u8)(sourceLength - maxCharacters);
                    }
                    if (viewStart + maxCharacters > sourceLength)
                        viewStart = (u8)(sourceLength - maxCharacters);
                }
                viewLength = (u8)(sourceLength - viewStart);
                if (viewLength > maxCharacters)
                    viewLength = maxCharacters;
                for (index = 0; index < viewLength; index++)
                    visible[index] = source[viewStart + index];
                visible[viewLength] = '\0';
            }

            textWidth = (s16)(viewLength * GFX_ASCII5X7_ADVANCE - 1);
            textX = (s16)(cellLeft + (cellWidth - textWidth) / 2);
            gfxText5x7(surface, textX, textY, visible,
                       sourceLength == 0 ? APP_COLOR_MUTED :
                                           APP_COLOR_TEXT);

            if (selected && app->cursorVisible) {
                s16 cursorX;

                if (sourceLength == 0) {
                    cursorX = (s16)(textX - 2);
                } else if (app->gridCursor.offset <= viewStart) {
                    cursorX = textX;
                } else if (app->gridCursor.offset >=
                           viewStart + viewLength) {
                    cursorX = (s16)(textX +
                                    viewLength * GFX_ASCII5X7_ADVANCE);
                } else {
                    cursorX = (s16)(textX +
                                    (app->gridCursor.offset - viewStart) *
                                    GFX_ASCII5X7_ADVANCE);
                }
                if (cursorX < cellLeft + 1)
                    cursorX = (s16)(cellLeft + 1);
                if (cursorX > cellRight - 2)
                    cursorX = (s16)(cellRight - 2);
                gfxLine(surface, cursorX, textY, cursorX,
                        (s16)(textY + GFX_ASCII5X7_HEIGHT - 1),
                        APP_COLOR_CURSOR);
            }
        }
    }
}

static void appRenderGridCells(AppState *app, GfxSurface *surface)
{
    const AppActionSpec *spec = appCurrentAction(app);
    u8 panel = app->gridPanel < app->gridPanelCount ? app->gridPanel : 0;
    u8 rows = app->gridRows[panel];
    u8 columns = app->gridColumns[panel];
    u8 row;
    u8 column;
    char shape[12];

    gfxFillRect(surface, 0, APP_FORM_Y, GBA_SCREEN_WIDTH,
                APP_GRID_RESULT_Y - APP_FORM_Y, APP_COLOR_BACKGROUND);
    if (spec == 0 || rows == 0 || columns == 0)
        return;
    appDrawClippedText(surface, 3, 15, 126, spec->label,
                       APP_COLOR_ACCENT);
    appCopyText(shape, sizeof(shape), appGridPanelLabel(app));
    appAppendText(shape, sizeof(shape), " ");
    {
        u8 length = (u8)strlen(shape);
        if ((u16)length + 3u < (u16)sizeof(shape)) {
            shape[length++] = (char)('0' + rows);
            shape[length++] = 'x';
            shape[length++] = (char)('0' + columns);
            shape[length] = '\0';
        }
    }
    appDrawClippedText(surface, 132, 15, 56, shape, APP_COLOR_MUTED);
    if (app->mode == CALC_MODE_INEQ) {
        appDrawClippedText(surface, 191, 15, 46,
                           appGridRelationLabel(app->gridRelation),
                           APP_COLOR_CURSOR);
    }
    if (appGridUsesTextbookBrackets(app, panel)) {
        appRenderTextbookGridCells(app, surface, panel, rows, columns);
        return;
    }
    for (row = 0; row < rows; row++) {
        s16 y = (s16)(APP_GRID_TABLE_Y +
                      (row * APP_GRID_TABLE_HEIGHT) / rows);
        s16 bottom = (s16)(APP_GRID_TABLE_Y +
                           ((row + 1) * APP_GRID_TABLE_HEIGHT) / rows);
        for (column = 0; column < columns; column++) {
            s16 x = (s16)((column * GBA_SCREEN_WIDTH) / columns);
            s16 right = (s16)(((column + 1) * GBA_SCREEN_WIDTH) /
                              columns);
            s16 width = (s16)(right - x);
            u8 selected = row == app->gridRow &&
                          column == app->gridColumn;
            u16 fill = selected ? APP_COLOR_PANEL_ALT : APP_COLOR_PANEL;
            s16 cursorX;

            gfxFillRect(surface, x, y, width, (s16)(bottom - y), fill);
            gfxRect(surface, x, y, width, (s16)(bottom - y),
                    APP_COLOR_GRID);
            appDrawClippedText(surface, (s16)(x + 2), (s16)(y + 2),
                               (s16)(width - 4),
                               app->gridCells[panel][row][column],
                               APP_COLOR_TEXT);
            if (!selected || !app->cursorVisible)
                continue;
            cursorX = (s16)(x + 2 + app->gridCursor.offset *
                           GFX_ASCII5X7_ADVANCE);
            if (cursorX > right - 2)
                cursorX = (s16)(right - 2);
            gfxLine(surface, cursorX, (s16)(y + 2), cursorX,
                    (s16)(bottom - 2), APP_COLOR_CURSOR);
        }
    }
}

static void appRenderGridResult(AppState *app, GfxSurface *surface)
{
    u16 color = app->runtime.error == CALC_OK ? APP_COLOR_ACCENT :
                                                APP_COLOR_ERROR;

    gfxFillRect(surface, 0, APP_GRID_RESULT_Y, GBA_SCREEN_WIDTH,
                APP_GRID_RESULT_HEIGHT, APP_COLOR_PANEL);
    appDrawClippedText(surface, 3, APP_GRID_RESULT_Y + 2, 234,
                       app->runtime.result, color);
    gfxFillRect(surface, 0, APP_GRID_STATUS_Y, GBA_SCREEN_WIDTH,
                APP_GRID_STATUS_HEIGHT, APP_COLOR_BACKGROUND);
    appDrawClippedText(surface, 3, APP_GRID_STATUS_Y + 1, 234,
                       app->status, APP_COLOR_MUTED);
}

static void appRenderModeGrid(AppState *app, GfxSurface *surface,
                              u8 full, u32 dirty)
{
    if (full || (dirty & APP_DIRTY_HEADER) != 0)
        appRenderHeader(app, surface);
    if (full || (dirty & (APP_DIRTY_EXPRESSION |
                          APP_DIRTY_VIEWPORT)) != 0)
        appRenderGridCells(app, surface);
    if (full || (dirty & (APP_DIRTY_RESULT | APP_DIRTY_STATUS)) != 0)
        appRenderGridResult(app, surface);
    if (full || (dirty & APP_DIRTY_KEYPAD) != 0)
        appRenderSharedKeypadAt(app, surface, full, APP_GRID_KEYPAD_Y,
                                APP_GRID_KEYPAD_HEIGHT);
}

static void appRenderStatDataTable(AppState *app, GfxSurface *surface)
{
    static const char *const labels[3] = {"X", "Y", "FREQ"};
    u8 columns = appStatColumnCount(app);
    u8 row;
    u8 column;

    gfxFillRect(surface, 0, APP_STAT_TABLE_Y, GBA_SCREEN_WIDTH,
                APP_STAT_TABLE_HEIGHT, APP_COLOR_BACKGROUND);
    for (column = 0; column < columns; column++) {
        u8 logical = columns == 2 && column == 1 ? 2 : column;
        s16 x = (s16)((column * GBA_SCREEN_WIDTH) / columns);
        s16 right = (s16)(((column + 1) * GBA_SCREEN_WIDTH) / columns);
        appDrawClippedText(surface, (s16)(x + 3), 16,
                           (s16)(right - x - 6), labels[logical],
                           APP_COLOR_ACCENT);
    }
    for (row = 0; row < 3; row++) {
        u8 sourceRow = (u8)(app->statScroll + row);
        s16 y = (s16)(25 + row * 11);
        for (column = 0; column < columns; column++) {
            s16 x = (s16)((column * GBA_SCREEN_WIDTH) / columns);
            s16 right = (s16)(((column + 1) * GBA_SCREEN_WIDTH) /
                              columns);
            s16 width = (s16)(right - x);
            u8 selected = sourceRow == app->statRow &&
                          column == app->statColumn;
            s16 cursorX;

            gfxFillRect(surface, x, y, width, 11,
                        selected ? APP_COLOR_PANEL_ALT : APP_COLOR_PANEL);
            gfxRect(surface, x, y, width, 11, APP_COLOR_GRID);
            appDrawClippedText(surface, (s16)(x + 2), (s16)(y + 2),
                               (s16)(width - 4),
                               app->statCells[sourceRow][column],
                               APP_COLOR_TEXT);
            if (!selected || !app->cursorVisible)
                continue;
            cursorX = (s16)(x + 2 + app->formCursor.offset *
                           GFX_ASCII5X7_ADVANCE);
            if (cursorX > right - 2)
                cursorX = (s16)(right - 2);
            gfxLine(surface, cursorX, (s16)(y + 2), cursorX,
                    (s16)(y + 8), APP_COLOR_CURSOR);
        }
    }
}

static void appRenderStatData(AppState *app, GfxSurface *surface,
                              u8 full, u32 dirty)
{
    if (full || (dirty & APP_DIRTY_HEADER) != 0)
        appRenderHeader(app, surface);
    if (full || (dirty & (APP_DIRTY_EXPRESSION |
                          APP_DIRTY_VIEWPORT)) != 0)
        appRenderStatDataTable(app, surface);
    if (full || (dirty & (APP_DIRTY_STATUS | APP_DIRTY_RESULT)) != 0) {
        const char *text = app->runtime.result[0] != '\0' ?
                           app->runtime.result : app->status;
        u16 color = app->runtime.error == CALC_OK ? APP_COLOR_MUTED :
                                                    APP_COLOR_ERROR;
        gfxFillRect(surface, 0, APP_STAT_STATUS_Y, GBA_SCREEN_WIDTH,
                    APP_STAT_STATUS_HEIGHT, APP_COLOR_BACKGROUND);
        appDrawClippedText(surface, 3, APP_STAT_STATUS_Y + 1, 234,
                           text, color);
    }
    if (full || (dirty & APP_DIRTY_KEYPAD) != 0)
        appRenderSharedKeypadAt(app, surface, full, APP_STAT_KEYPAD_Y,
                                APP_STAT_KEYPAD_HEIGHT);
}

static void appRenderBaseCell(AppState *app, GfxSurface *surface, u8 index)
{
    s16 column = (s16)(index % APP_KEYPAD_COLUMNS);
    s16 row = (s16)(index / APP_KEYPAD_COLUMNS);
    s16 x = (s16)(column * (GBA_SCREEN_WIDTH / APP_KEYPAD_COLUMNS));
    s16 y = (s16)(APP_KEYPAD_Y +
                  (row * APP_KEYPAD_HEIGHT) / APP_KEYPAD_ROWS);
    s16 width = GBA_SCREEN_WIDTH / APP_KEYPAD_COLUMNS;
    s16 bottom = (s16)(APP_KEYPAD_Y +
                       ((row + 1) * APP_KEYPAD_HEIGHT) /
                       APP_KEYPAD_ROWS);
    s16 height = (s16)(bottom - y);
    u8 page = app->tokenPage < APP_BASE_PAGES ? app->tokenPage : 0;
    const char *label = appBaseTokens[page][index].label;
    u8 selected = index == app->selectedToken;
    u8 execute = index == APP_KEYPAD_EXE;
    u8 enabled = appBaseTokenEnabled(app, index);
    u16 fill = selected ? APP_COLOR_ACCENT :
               (execute ? APP_COLOR_CURSOR : APP_COLOR_PANEL_ALT);
    u16 color = selected || execute ? APP_COLOR_BACKGROUND :
                (enabled ? APP_COLOR_TEXT : APP_COLOR_MUTED);
    s16 textX = (s16)(x + (width - strlen(label) *
                           GFX_ASCII5X7_ADVANCE) / 2);

    gfxFillRect(surface, x, y, width, height, fill);
    gfxRect(surface, x, y, width, height, APP_COLOR_BACKGROUND);
    gfxText5x7(surface, textX,
               (s16)(y + (height - GFX_ASCII5X7_HEIGHT) / 2),
               label, color);
}

static void appRenderBaseKeypad(AppState *app, GfxSurface *surface, u8 full)
{
    u8 index;
    if (full || app->menuSelection == 0xff) {
        for (index = 0; index < APP_TOKENS_PER_PAGE; index++)
            appRenderBaseCell(app, surface, index);
    } else {
        if (app->menuSelection < APP_TOKENS_PER_PAGE)
            appRenderBaseCell(app, surface, app->menuSelection);
        appRenderBaseCell(app, surface, app->selectedToken);
    }
}

static void appRenderBase(AppState *app, GfxSurface *surface, u8 full,
                          u32 dirty)
{
    if (full || (dirty & APP_DIRTY_HEADER) != 0)
        appRenderHeader(app, surface);
    if (full || (dirty & APP_DIRTY_EXPRESSION) != 0)
        appRenderNaturalExpression(surface, app->baseExpression,
                                   app->baseExpressionLength,
                                   &app->baseCursor, app->cursorVisible,
                                   APP_EXPRESSION_Y,
                                   APP_EXPRESSION_HEIGHT);
    if (full || (dirty & APP_DIRTY_RESULT) != 0)
        appRenderResult(app, surface);
    if (full || (dirty & APP_DIRTY_STATUS) != 0)
        appRenderStatus(app, surface);
    if (full || (dirty & APP_DIRTY_KEYPAD) != 0)
        appRenderBaseKeypad(app, surface, full);
}

static void appRenderGraphInputCell(AppState *app, GfxSurface *surface,
                                    u8 index)
{
    s16 column = (s16)(index % APP_GRAPH_INPUT_COLUMNS);
    s16 row = (s16)(index / APP_GRAPH_INPUT_COLUMNS);
    s16 width = GBA_SCREEN_WIDTH / APP_GRAPH_INPUT_COLUMNS;
    s16 height = APP_GRAPH_INPUT_KEYPAD_HEIGHT / APP_GRAPH_INPUT_ROWS;
    s16 x = (s16)(column * width);
    s16 y = (s16)(APP_GRAPH_INPUT_KEYPAD_Y + row * height);
    u8 selected = index == app->graphSelectedToken;
    u8 execute = index == APP_GRAPH_INPUT_EXE;
    u16 fill = selected ? APP_COLOR_ACCENT :
               (execute ? APP_COLOR_CURSOR : APP_COLOR_PANEL_ALT);
    u16 text = selected || execute ? APP_COLOR_BACKGROUND : APP_COLOR_TEXT;
    const char *label = appGraphTokens[app->graphTokenPage][index].label;
    s16 labelWidth = (s16)(strlen(label) * GFX_ASCII5X7_ADVANCE);
    s16 textX = (s16)(x + (width - labelWidth) / 2);

    gfxFillRect(surface, x, y, width, height, fill);
    gfxRect(surface, x, y, width, height, APP_COLOR_BACKGROUND);
    gfxText5x7(surface, textX,
               (s16)(y + (height - GFX_ASCII5X7_HEIGHT) / 2),
               label, text);
}

static void appRenderGraphInputKeypad(AppState *app, GfxSurface *surface,
                                      u8 full)
{
    u8 index;

    if (full || app->menuSelection == 0xff) {
        for (index = 0; index < APP_GRAPH_INPUT_TOKENS; index++)
            appRenderGraphInputCell(app, surface, index);
    } else {
        if (app->menuSelection < APP_GRAPH_INPUT_TOKENS)
            appRenderGraphInputCell(app, surface, app->menuSelection);
        appRenderGraphInputCell(app, surface, app->graphSelectedToken);
    }
}

static void appRenderGraphInput(AppState *app, GfxSurface *surface,
                                u8 full, u32 dirty)
{
    if (full || (dirty & APP_DIRTY_HEADER) != 0)
        appRenderHeader(app, surface);
    if (full || (dirty & APP_DIRTY_EXPRESSION) != 0)
        appRenderNaturalExpression(surface, app->graphExpression,
                                   app->graphExpressionLength,
                                   &app->graphCursor, app->cursorVisible,
                                   APP_GRAPH_INPUT_EXPRESSION_Y,
                                   APP_GRAPH_INPUT_EXPRESSION_HEIGHT);
    if (full || (dirty & APP_DIRTY_STATUS) != 0) {
        gfxFillRect(surface, 0, APP_GRAPH_INPUT_STATUS_Y,
                    GBA_SCREEN_WIDTH, APP_GRAPH_INPUT_STATUS_HEIGHT,
                    APP_COLOR_BACKGROUND);
        gfxText5x7(surface, 3, APP_GRAPH_INPUT_STATUS_Y + 1,
                   app->status, app->runtime.error == CALC_OK ?
                   APP_COLOR_MUTED : APP_COLOR_ERROR);
    }
    if (full || (dirty & APP_DIRTY_KEYPAD) != 0)
        appRenderGraphInputKeypad(app, surface, full);
}

static void appRenderMenuCell(AppState *app, GfxSurface *surface, u8 index)
{
    s16 column = (s16)(index % 3);
    s16 row = (s16)(index / 3);
    s16 x = (s16)(column * 80);
    s16 y = (s16)(20 + row * 34);
    u8 selected = index == app->menuSelection;
    u16 fill = selected ? APP_COLOR_ACCENT : APP_COLOR_PANEL;
    u16 text = selected ? APP_COLOR_BACKGROUND : APP_COLOR_TEXT;

    gfxFillRect(surface, x + 1, y, 78, 30, fill);
    gfxRect(surface, x + 1, y, 78, 30, APP_COLOR_PANEL_ALT);
    gfxText5x7(surface, x + 4, y + 11,
               modeLabel((CalcMode)index), text);
}

static void appRenderMenu(AppState *app, GfxSurface *surface, u8 full,
                          u32 dirty)
{
    u8 index;

    if (full || (dirty & APP_DIRTY_HEADER) != 0)
        appRenderHeader(app, surface);
    if (full || (dirty & APP_DIRTY_KEYPAD) == 0) {
        for (index = 0; index < CALC_MODE_COUNT; index++)
            appRenderMenuCell(app, surface, index);
    } else {
        if (app->selectedToken < CALC_MODE_COUNT)
            appRenderMenuCell(app, surface, app->selectedToken);
        appRenderMenuCell(app, surface, app->menuSelection);
    }
}

static void appRenderTable(AppState *app, GfxSurface *surface, u8 full,
                           u32 dirty)
{
    TableResult *table = &app->runtime.table;
    u8 row;
    u8 start = app->selectedToken;

    if (full || (dirty & APP_DIRTY_HEADER) != 0)
        appRenderHeader(app, surface);
    if (!full && (dirty & APP_DIRTY_VIEWPORT) == 0)
        return;
    gfxFillRect(surface, 0, APP_EXPRESSION_Y, GBA_SCREEN_WIDTH,
                GBA_SCREEN_HEIGHT - APP_EXPRESSION_Y,
                APP_COLOR_BACKGROUND);
    gfxText5x7(surface, 3, 17, "X", APP_COLOR_ACCENT);
    gfxText5x7(surface, 63, 17, "F(X)", APP_COLOR_ACCENT);
    if (table->functionCount == 2)
        gfxText5x7(surface, 123, 17, "G(X)", APP_COLOR_ACCENT);
    else if (table->hasDerivative)
        gfxText5x7(surface, 123, 17, "D/F", APP_COLOR_ACCENT);
    gfxLine(surface, 0, 26, 239, 26, APP_COLOR_AXIS);
    gfxLine(surface, 59, 14, 59, 159, APP_COLOR_GRID);
    gfxLine(surface, 119, 14, 119, 159, APP_COLOR_GRID);
    for (row = 0; row < 13 && start + row < table->rows; row++) {
        s16 y = (s16)(29 + row * 10);
        u8 source = (u8)(start + row);
        gfxText5x7(surface, 3, y, table->x[source], APP_COLOR_TEXT);
        gfxText5x7(surface, 63, y, table->y[source], APP_COLOR_TEXT);
        if (table->functionCount == 2)
            gfxText5x7(surface, 123, y, table->z[source], APP_COLOR_TEXT);
        else if (table->hasDerivative)
            gfxText5x7(surface, 123, y, table->derivative[source],
                       APP_COLOR_TEXT);
    }
}

static s16 appWorldToScreenX(const GraphViewport *viewport, s32 x)
{
    s64 width = (s64)viewport->xMax - viewport->xMin;
    if (width <= 0)
        return 0;
    return (s16)(((s64)(x - viewport->xMin) *
                  (GBA_SCREEN_WIDTH - 1)) / width);
}

static s16 appWorldToScreenY(const GraphViewport *viewport, s32 y)
{
    s64 height = (s64)viewport->yMax - viewport->yMin;
    if (height <= 0)
        return APP_GRAPH_Y;
    return (s16)(APP_GRAPH_Y +
          ((s64)(viewport->yMax - y) * (APP_GRAPH_HEIGHT - 1)) / height);
}

static s32 appGridStep(s32 span)
{
    s32 step = CALC_ONE;

    if (span < 0)
        span = -span;
    while (step < 0x20000000 && span / step > 12)
        step *= 2;
    while (step > 16 && span / step < 5)
        step /= 2;
    return step;
}

static s32 appFirstGrid(s32 minimum, s32 step)
{
    s32 quotient;
    if (step <= 0)
        return minimum;
    quotient = minimum / step;
    if (minimum > 0 && minimum % step != 0)
        quotient++;
    return quotient * step;
}

static void appRenderGraphGrid(AppState *app, GfxSurface *surface)
{
    GraphViewport viewport = app->graph.viewport;
    s32 xStep = appGridStep(viewport.xMax - viewport.xMin);
    s32 yStep = appGridStep(viewport.yMax - viewport.yMin);
    s32 value;
    u8 guard = 0;

    for (value = appFirstGrid(viewport.xMin, xStep);
         value <= viewport.xMax && guard++ < 32; value += xStep) {
        s16 x = appWorldToScreenX(&viewport, value);
        gfxLine(surface, x, APP_GRAPH_Y, x,
                APP_GRAPH_Y + APP_GRAPH_HEIGHT - 1,
                value == 0 ? APP_COLOR_AXIS : APP_COLOR_GRID);
    }
    guard = 0;
    for (value = appFirstGrid(viewport.yMin, yStep);
         value <= viewport.yMax && guard++ < 32; value += yStep) {
        s16 y = appWorldToScreenY(&viewport, value);
        gfxLine(surface, 0, y, GBA_SCREEN_WIDTH - 1, y,
                value == 0 ? APP_COLOR_AXIS : APP_COLOR_GRID);
    }
}

static u16 appShadeColor(u16 color)
{
    u16 red = color & 31;
    u16 green = (color >> 5) & 31;
    u16 blue = (color >> 10) & 31;
    return RGB15(red / 3, green / 3, blue / 3);
}

static void appDrawGraphSample(AppState *app, GfxSurface *surface,
                               const GraphFunction *function,
                               const GraphSample *sample, u16 color)
{
    s16 x;
    s16 y;

    if (sample->state != GRAPH_SAMPLE_VALID ||
        sample->xFixed < app->graph.viewport.xMin ||
        sample->xFixed > app->graph.viewport.xMax ||
        sample->yFixed < app->graph.viewport.yMin ||
        sample->yFixed > app->graph.viewport.yMax)
        return;
    x = appWorldToScreenX(&app->graph.viewport, sample->xFixed);
    y = appWorldToScreenY(&app->graph.viewport, sample->yFixed);
    if (function->row.kind == CALC_GRAPH_ROW_INEQUALITY &&
        function->row.relation[0] != '!') {
        u16 shade = appShadeColor(color);
        if (function->row.axisX) {
            if (function->row.relation[0] == '<')
                gfxLine(surface, 0, y, x, y, shade);
            else
                gfxLine(surface, x, y, GBA_SCREEN_WIDTH - 1, y, shade);
        } else {
            if (function->row.relation[0] == '<')
                gfxLine(surface, x, y, x,
                        APP_GRAPH_Y + APP_GRAPH_HEIGHT - 1, shade);
            else
                gfxLine(surface, x, APP_GRAPH_Y, x, y, shade);
        }
    }
    gfxPixel(surface, x, y, color);
}

static void appDrawGraphSegment(AppState *app, GfxSurface *surface,
                                const GraphSample *previous,
                                const GraphSample *sample, u16 color)
{
    s32 x0;
    s32 y0;
    s32 x1;
    s32 y1;

    if (sample->breakBefore || previous->state != GRAPH_SAMPLE_VALID ||
        sample->state != GRAPH_SAMPLE_VALID)
        return;
    if (!graphClipWorldSegment(previous->xFixed, previous->yFixed,
                               sample->xFixed, sample->yFixed,
                               app->graph.viewport.xMin,
                               app->graph.viewport.xMax,
                               app->graph.viewport.yMin,
                               app->graph.viewport.yMax,
                               &x0, &y0, &x1, &y1))
        return;
    gfxLine(surface, appWorldToScreenX(&app->graph.viewport, x0),
            appWorldToScreenY(&app->graph.viewport, y0),
            appWorldToScreenX(&app->graph.viewport, x1),
            appWorldToScreenY(&app->graph.viewport, y1), color);
}

static void appRenderGraphChunk(AppState *app, GfxSurface *surface)
{
    GraphSample samples[32];
    GraphSample previous = app->graph.previous;
    s32 localSample = app->graph.nextSample;
    u8 localFunction = app->graph.functionIndex;
    u8 havePrevious = app->graph.hasPrevious;
    u8 produced = 0;
    u8 index;

    if (app->graph.complete)
        return;
    while (localFunction < app->graph.functionCount &&
           (!app->graph.functions[localFunction].enabled ||
            !app->graph.functions[localFunction].valid)) {
        localFunction++;
        localSample = 0;
        havePrevious = 0;
    }
    if (graphJobStep(&app->graph, samples,
                     (u8)(sizeof(samples) / sizeof(samples[0])),
                     &produced) != CALC_OK) {
        appSetStatus(app, "PLOT ERROR");
        return;
    }
    for (index = 0; index < produced && localFunction <
         app->graph.functionCount; index++) {
        const GraphFunction *function = &app->graph.functions[localFunction];
        u16 color = appGraphColors[localFunction % GRAPH_MAX_FUNCTIONS];

        appDrawGraphSample(app, surface, function, &samples[index], color);
        if (havePrevious)
            appDrawGraphSegment(app, surface, &previous, &samples[index],
                                color);
        previous = samples[index];
        havePrevious = 1;
        localSample++;
        if (localSample >= app->graph.sampleCount) {
            localSample = 0;
            localFunction++;
            havePrevious = 0;
            while (localFunction < app->graph.functionCount &&
                   (!app->graph.functions[localFunction].enabled ||
                    !app->graph.functions[localFunction].valid))
                localFunction++;
        }
    }
    if (app->graph.complete && !app->traceActive)
        appSetStatus(app, "PLOT COMPLETE");
}

static void appRenderTrace(AppState *app, GfxSurface *surface)
{
    GraphFunction *function;
    GraphViewport viewport;
    GraphSampleState state;
    s32 parameter;
    s32 x;
    s32 y;
    s16 screenX;
    s16 screenY;

    if (!app->traceActive || app->graphFunctionCount == 0)
        return;
    if (app->selectedToken >= app->graphFunctionCount)
        app->selectedToken = 0;
    function = &app->graphFunctions[app->selectedToken];
    viewport = app->graph.viewport;
    if (function->row.kind == CALC_GRAPH_ROW_X ||
        (function->row.kind == CALC_GRAPH_ROW_INEQUALITY &&
         function->row.axisX)) {
        parameter = (s32)(viewport.yMin +
                    ((s64)(viewport.yMax - viewport.yMin) *
                     app->cursor.offset) / (GBA_SCREEN_WIDTH - 1));
    } else if (function->row.kind == CALC_GRAPH_ROW_POLAR ||
               function->row.kind == CALC_GRAPH_ROW_PARAM) {
        s32 turn = 1608;
        if (app->runtime.calc.angleMode == CALC_ANGLE_DEG)
            turn = 360 * CALC_ONE;
        else if (app->runtime.calc.angleMode == CALC_ANGLE_GRAD)
            turn = 400 * CALC_ONE;
        parameter = (s32)(((s64)turn * app->cursor.offset) /
                          (GBA_SCREEN_WIDTH - 1));
    } else {
        parameter = (s32)(viewport.xMin +
                    ((s64)(viewport.xMax - viewport.xMin) *
                     app->cursor.offset) / (GBA_SCREEN_WIDTH - 1));
    }
    state = graphEvaluatePoint(function, &app->runtime.calc, parameter,
                               &x, &y);
    if (state != GRAPH_SAMPLE_VALID || x < viewport.xMin ||
        x > viewport.xMax || y < viewport.yMin || y > viewport.yMax)
        return;
    screenX = appWorldToScreenX(&viewport, x);
    screenY = appWorldToScreenY(&viewport, y);
    gfxLine(surface, (s16)(screenX - 3), screenY,
            (s16)(screenX + 3), screenY, APP_COLOR_CURSOR);
    gfxLine(surface, screenX, (s16)(screenY - 3), screenX,
            (s16)(screenY + 3), APP_COLOR_CURSOR);
}

static void appRenderGraph(AppState *app, GfxSurface *surface, u8 full,
                           u32 dirty)
{
    if (full || (dirty & APP_DIRTY_HEADER) != 0)
        appRenderHeader(app, surface);
    if (full || (dirty & APP_DIRTY_VIEWPORT) != 0) {
        gfxFillRect(surface, 0, APP_GRAPH_Y, GBA_SCREEN_WIDTH,
                    APP_GRAPH_HEIGHT, APP_COLOR_BACKGROUND);
        appRenderGraphGrid(app, surface);
    }
    appRenderGraphChunk(app, surface);
    appRenderTrace(app, surface);
    if (full || (dirty & APP_DIRTY_STATUS) != 0) {
        gfxFillRect(surface, 0, APP_GRAPH_STATUS_Y, GBA_SCREEN_WIDTH,
                    APP_GRAPH_STATUS_HEIGHT, APP_COLOR_PANEL);
        gfxText5x7(surface, 3, APP_GRAPH_STATUS_Y + 1, app->status,
                   APP_COLOR_MUTED);
    }
}

void appRender(AppState *app, GfxSurface *surface)
{
    u32 dirty;
    u8 full;

    if (app == 0 || surface == 0 || surface->pixels == 0)
        return;
    dirty = app->dirty;
    app->dirty = APP_DIRTY_NONE;
    full = (u8)((dirty & APP_DIRTY_ALL) == APP_DIRTY_ALL);
    if (full)
        gfxClear(surface, APP_COLOR_BACKGROUND);

    switch (app->view) {
    case APP_VIEW_MENU:
        appRenderMenu(app, surface, full, dirty);
        break;
    case APP_VIEW_TABLE:
        appRenderTable(app, surface, full, dirty);
        break;
    case APP_VIEW_GRAPH:
        appRenderGraph(app, surface, full, dirty);
        break;
    case APP_VIEW_GRAPH_INPUT:
        appRenderGraphInput(app, surface, full, dirty);
        break;
    case APP_VIEW_MODE_ACTION:
        appRenderModeActions(app, surface, full, dirty);
        break;
    case APP_VIEW_MODE_FORM:
        appRenderModeForm(app, surface, full, dirty);
        break;
    case APP_VIEW_MODE_GRID:
        appRenderModeGrid(app, surface, full, dirty);
        break;
    case APP_VIEW_STAT_TYPE:
        appRenderStatTypes(app, surface, full, dirty);
        break;
    case APP_VIEW_STAT_DATA:
        appRenderStatData(app, surface, full, dirty);
        break;
    case APP_VIEW_BASEN:
        appRenderBase(app, surface, full, dirty);
        break;
    case APP_VIEW_CALCULATOR:
    default:
        if (full || (dirty & APP_DIRTY_HEADER) != 0)
            appRenderHeader(app, surface);
        if (full || (dirty & APP_DIRTY_EXPRESSION) != 0)
            appRenderExpression(app, surface);
        if (full || (dirty & APP_DIRTY_RESULT) != 0)
            appRenderResult(app, surface);
        if (full || (dirty & APP_DIRTY_STATUS) != 0)
            appRenderStatus(app, surface);
        if (full || (dirty & APP_DIRTY_KEYPAD) != 0)
            appRenderKeypad(app, surface, full);
        break;
    }
}
