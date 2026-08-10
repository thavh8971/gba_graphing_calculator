# GBA Graphing Calculator


<img width="476" height="374" alt="image" src="https://github.com/user-attachments/assets/c424f61c-3c42-4b79-b6dc-3f1e75a4f4ef" />

<img width="475" height="366" alt="image" src="https://github.com/user-attachments/assets/a88061b8-4b49-4922-b858-6b5dec00b4a3" />

<img width="476" height="370" alt="image" src="https://github.com/user-attachments/assets/df4e6769-0bc8-4454-84f1-c9b95d835e5e" />

<img width="473" height="367" alt="image" src="https://github.com/user-attachments/assets/b0b6cd2a-ed8a-49b1-98d1-396fc9ccdbe6" />


A graphing-calculator prototype rewritten with a modular architecture for the Game Boy Advance. The target ROM runs at 240×160 in Mode 3, drawing RGB15 pixels directly into the VRAM framebuffer. The parser, evaluator, modes, and graph sampler can all be built on the host without libgba; only the platform layer touches GBA hardware.

This is a functional prototype, not Casio firmware, and it does not claim to be a complete clone of any particular model. The project also does not claim to implement 453 functions/commands.

## What is in the repository

```text
include/gcalc/            public C API
source/core/              parser-independent number/evaluator/decimal core
source/modes/             dispatcher and per-mode implementations
source/graph/             typed graph rows, streaming sampler, world clipping
source/ui/                framebuffer primitives, font, Natural cursor
source/app/               shared state machine and renderer
source/platform/          GBA entry point + libgba key/VBlank bridge
tests/                    host test executables
scripts/build.ps1         Windows build/verify entry point
docs/                     contracts, architecture, and toolchain snapshot
```

Detailed design information is available in [docs/architecture.md](https://github.com/thavh8971/gba_graphing_calculator/blob/main/docs/architecture.md). The full syntax and the boundary between this project and Casio documentation are described in [docs/reference_matrix.md](https://github.com/thavh8971/gba_graphing_calculator/blob/main/docs/reference_matrix.md).

## Build requirements

On Windows:

* PowerShell 5.1 or newer;
* GNU Make and an MSYS2-style shell/utilities environment;
* host GCC for running host tests;
* devkitARM, libgba, and `gbafix` from devkitPro to build the ROM.

`scripts/build.ps1` automatically searches `C:\devkitPro` and `C:\msys64\opt\devkitpro`; these paths can be overridden with parameters. The reference package snapshot is recorded in [docs/toolchain.lock.md](https://github.com/thavh8971/gba_graphing_calculator/blob/main/docs/toolchain.lock.md). Toolchain binaries are not vendored in the repository.

## Build and verify

Run these commands from the project root in PowerShell:

```powershell
# Build and run all host tests
.\scripts\build.ps1 host-test

# Build the ROM
.\scripts\build.ps1 gba

# Rebuild host tests + ROM, then verify the actual cartridge header
.\scripts\build.ps1 verify -Rebuild

# Print metadata, size, and SHA-256 of the newly built ROM
.\scripts\build.ps1 rom-info
```

If the toolchain is installed somewhere else:

```powershell
.\scripts\build.ps1 verify -Rebuild `
  -DevkitPro C:\msys64\opt\devkitpro `
  -Make C:\msys64\usr\bin\make.exe
```

Artifacts are generated under:

```text
gba_graphing_calculator.gba                 release ROM to open/run
build/gba/gba_graphing_calculator.gba
build/gba/gba_graphing_calculator.elf
build/gba/gba_graphing_calculator.map
build/host/*_test.exe
```

The ROM in the project root is always copied byte-for-byte from `build/gba` by the `gba`, `verify`, and `rom-info` targets; this is the canonical path to avoid accidentally opening an outdated ROM.

This README intentionally does not copy size, checksum, or the word “PASS” from an older project. Handoff values must be obtained by running `verify`/`rom-info` on the current checkout itself.

## Controls

### COMP/CMPLX expression editor

| Key                | Behavior                                                                                                |
| ------------------ | ------------------------------------------------------------------------------------------------------- |
| D-pad              | Move the selection; crossing a grid edge changes the page while preserving the corresponding row/column |
| A                  | Insert a token; if `EXE` is selected, execute the current mode                                          |
| B                  | Delete the character immediately before the cursor                                                      |
| L / R              | Move the expression cursor left/right                                                                   |
| SELECT + UP / DOWN | Move between structural slots vertically within fraction, power, root, or calculus templates            |
| B + LEFT / RIGHT   | Change keypad page                                                                                      |
| START              | Shortcut equivalent to selecting `EXE` and pressing A                                                   |
| SELECT + START     | Open/close the mode menu                                                                                |

The editor still stores expressions as linear text. The Natural cursor recognizes numerator, denominator, base, exponent, root index/radicand, body/variable/bounds, and derivative locations. The parser therefore always receives the same source string, while the UI can provide textbook-style layout without maintaining two different mathematical representations.

Every keypad is a 6×5 grid with `EXE` fixed at cell 29. Pages are filtered by mode rather than sharing one global catalogue: COMP has 7 pages and CMPLX has 8 pages; the other workspaces have 2–5 pages appropriate to their data. Crossing a grid edge with the D-pad changes pages cyclically while preserving the row/column on the other axis.

All text rendered by the application—headers, expressions, results, status, mode menu, table, keypad, and GRAPH—uses printable ASCII 5×7 glyphs. The old 5×9 API is retained only for compatibility at the graphics layer and is no longer called by the application UI.

### Mode menu

| Key            | Behavior                                  |
| -------------- | ----------------------------------------- |
| D-pad          | Select one of 12 modes on a 3×4 grid      |
| A / START      | Confirm the selected mode                 |
| B              | Close the menu and keep the previous mode |
| SELECT + START | Close the menu                            |

Selecting mode 12 opens a dedicated GRAPH expression input screen. The GRAPH expression is kept in a separate buffer from COMP and remains intact when switching between the editor and viewport.

### Per-mode workspaces

* STAT opens an 8-type selector, followed by an X/Freq table for 1-VAR or an X/Y/Freq table for regression. `SELECT+D-pad` changes the cell, `EXE` advances to the next cell, and `START` calculates.
* BASE-N has its own buffer, four choices—DEC/HEX/BIN/OCT—digit gating based on the radix, and two arithmetic/logic pages. `Neg` enters a 32-bit two's-complement value.
* EQN and INEQ open an action selector followed by a coefficient grid. MATRIX/VECTOR have four memory areas, `MatA..MatD` / `VctA..VctD`: select `EDIT` to open a textbook-style table with one large pair of square brackets, `SELECT+D-pad` changes cells, `SELECT+L/R` changes dimensions, `SELECT+B` clears, and `B` or `START` saves and exits. Matrix/vector operations open a pre-filled `f()` form containing the selected memory-area name.
* TABLE, RATIO, and DIST open an action selector followed by a form. `SELECT+UP/DOWN` changes fields, while `START` or `EXE` calculates. `B` on an empty field returns to the selector.

Workspaces maintain separate buffers from COMP and GRAPH. Form/grid operations serialize into a canonical command string at the mode-engine boundary; STAT sends row arrays directly, while `Mat`/`Vct` editors calculate each cell and write directly into fixed memory areas, avoiding dependence on a large command buffer.

### GRAPH expression input

| Key                | Behavior                                                                                                               |
| ------------------ | ---------------------------------------------------------------------------------------------------------------------- |
| D-pad              | Move the selection; crossing a grid edge moves to the previous/next page while preserving the corresponding row/column |
| A                  | Insert a token; if `EXE` is selected, parse and plot the current expression                                            |
| B                  | Delete the character immediately before the cursor                                                                     |
| L / R              | Move the expression cursor left/right                                                                                  |
| SELECT + UP / DOWN | Move between structural slots vertically                                                                               |
| B + LEFT / RIGHT   | Change page directly                                                                                                   |
| START              | Shortcut equivalent to selecting `EXE` and pressing A                                                                  |
| SELECT + START     | Open the mode menu                                                                                                     |

The GRAPH input keypad has four pages: `PLOT`, `FUNC`, `CALC`, and `SYM`. Each page is a 6×5 grid; the bottom-right cell (index 29) is always `EXE` and never inserts text into the expression. All characters in GRAPH input, the graph header, and graph status use 5×7 glyphs.

### Graph view

| Key                           | Behavior                                                                |
| ----------------------------- | ----------------------------------------------------------------------- |
| START                         | Re-parse the function list and start a new plot                         |
| D-pad when trace is off       | Pan the viewport                                                        |
| L / R                         | Zoom out / in                                                           |
| A                             | Toggle trace                                                            |
| LEFT / RIGHT when trace is on | Move the trace between samples                                          |
| UP / DOWN when trace is on    | Switch the function being traced                                        |
| B                             | Return to GRAPH expression input while preserving the expression/cursor |
| SELECT + START                | Open the mode menu                                                      |

The default viewport is centered at (0,0), with `x=[-10,10]`; the 240×134 plot area corresponds to approximately `y=[-5.58,5.58]`. Each function uses 240 samples, and the renderer processes at most 32 samples per frame, so the UI does not wait for the entire function list to finish before continuing through the VBlank loop.

### Table view

| Key            | Behavior                                                                                                 |
| -------------- | -------------------------------------------------------------------------------------------------------- |
| UP / DOWN      | Scroll through rows when the table is longer than the visible area                                       |
| START          | Recalculate the table from the current expression                                                        |
| B              | Return to the TABLE form if the table was created from a form; otherwise return to the expression editor |
| SELECT + START | Open the mode menu                                                                                       |

## Expressions

The main grammar accepts:

```text
+ - * / ^
! %
( )
= == != < <= > >=
2x  xy  2sin(x)  2(1+x)
1.25  1,25  1E100  1E-100
```

Examples:

```text
2A+3(4+1)
-2^2+5!+50%
sin(pi/2)
sum(x^2;x;1;3)
prod(x;x;1;4)
integral(x^2;x;0;1)
d/dx(x^2;x;3)
d2/dx2(x^2;x;3)
```

Use `;` to separate multiple arguments. A comma is also accepted as a decimal separator, so `;` is unambiguous for manually entered multi-argument expressions.

The public evaluator uses `CalcNumber`: nine significant digits with a decimal exponent range of -100..100. `DecimalNumber`, with 32-digit precision, is a separate module and does not yet replace the general-purpose evaluator. The default context is RAD; the core API supports DEG/GRAD, but the baseline UI does not assign a key for changing the angle mode.

## Twelve modes

Mode menu:

```text
1: COMP      2: CMPLX     3: STAT       4: BASE-N
5: EQN       6: MATRIX    7: TABLE      8: VECTOR
9: INEQ     10: RATIO    11: DIST      12: GRAPHING
```

The engine accepts compact command syntax; the UI provides per-mode editors and serializes them into this syntax:

| Mode     | Example contract                                                        |
| -------- | ----------------------------------------------------------------------- |
| COMP     | `sum(x^2;x;1;3)`, `ncr(10;3)`, `normalpdf(0;0;1)`                       |
| CMPLX    | `3+4i`, `conj:3+4i`, `pow:1+i;2`, `polar:2;pi/4`                        |
| STAT     | `1;2;3;4`, `170,66;173,68;179,75`, `freq:1,2,3;2,1,1`                   |
| BASE-N   | `bin(1011)`, `hex(FF)`, `101+1`, `xnor(15;7)`, `neg(1)`                 |
| EQN      | `solve:x^2=2;1`, `solven:x^2=1;-2;2`, `lin:1,1,3;2,-1,0`, `poly:1;-3;2` |
| MATRIX   | `det(MatA)`, `inv(MatA)`, `transpose(MatA)`, `mul(MatA;MatB)`           |
| TABLE    | `x^2;0;5;1`, `x;x^2;0;5;1`, `dtable:x^2;0;5;1`                          |
| VECTOR   | `norm(VctA)`, `dot(VctA;VctB)`, `cross(VctA;VctB)`, `scale(2;VctA)`     |
| INEQ     | `2*x<4`, `ineq2(1;0;-4;<)`, `ineq3(1;0;0;0;<)`, `ineq4(1;0;0;0;-1;<)`   |
| RATIO    | `1.5:2.5`, `2:3=8:?`                                                    |
| DIST     | `binomcdf(5;10;0.5)`, `normcdf(-1;1;1;0)`, `norminv(0.5;1;0)`           |
| GRAPHING | `x^2;sin(x)`, `param(cos(t);sin(t))`, `x>=y^2`                          |

The list of aliases, shape/range limits, and canonical argument ordering is documented in [docs/reference_matrix.md](https://github.com/thavh8971/gba_graphing_calculator/blob/main/docs/reference_matrix.md).

## Graph pipeline

The function list accepts up to 10 rows:

```text
y=f(x)                 or f(x)
x=f(y)
r=f(t)
param(x(t);y(t))       (`param:x(t);y(t)` is a compatibility alias)
y<f(x), y<=f(x), y>f(x), y>=f(x)
x<f(y), x<=f(y), x>f(y), x>=f(y)
```

The sampler emits `GraphSample` values in chunks, including fixed-point coordinates, parameter values, domain/pole/overflow state, and `breakBefore`. The renderer clips segments in world coordinates before rasterization. For `tan(g(x))`, phase crossings at `π/2 + kπ` are tracked separately to avoid the common mistake of connecting across poles.

This remains finite-resolution sampling. Arbitrary discontinuities are handled through state, detectors, and heuristics rather than symbolic singularity analysis; an asymptote or very narrow feature may still be incorrectly connected or missed.

## RAM and framebuffer

* The Mode 3 framebuffer occupies 76,800 bytes of VRAM;
* there is no full-screen shadow framebuffer in EWRAM;
* `AppState`, parse cache, and fallback AST storage are placed in EWRAM;
* the graph stream processes at most 32 samples per frame and does not retain the entire plot;
* expressions, ASTs, tables, function lists, and results all use fixed capacities;
* the project source does not directly call `malloc`/`free`; however, the ARM artifact currently links `snprintf`/`strtold` from newlib and consequently pulls in allocator/heap support, so this is not a hard no-heap build.

Some graph/mode/decimal workspaces use relatively large local variables. `verify-gba-memory` catches incorrect cache sections, ARM fast-fill targeting the wrong region/state, and stack margins below 18 KiB; host tests cannot detect timing/VRAM issues or visual correctness on actual GBA hardware.

## Verification boundary

The current host test suites cover:

* parser/evaluator, context isolation, and AST cache behavior under repeated/mutated input;
* decimal arithmetic and Natural cursor behavior;
* routing/page/keypad behavior for each workspace, forms/grids, STAT, BASE-N, and GRAPH EXE/B behavior;
* mode runtime/aliases, graph row classification, streaming, clipping, and framebuffer stride/guard behavior.

`verify-rom-header` also reads back the title/game code/maker/version from the ROM after `gbafix`. None of these steps proves the actual rendered image or physical input behavior on an emulator or real hardware. This documentation baseline does not include visual emulator verification, and the development process did not use Computer Use/emulator interaction.

## Current limitations

* not a complete clone of fx-9750GII/GIII, fx-570VN PLUS, or any other Casio model;
* `DecimalNumber` is not yet the common backend;
* DEG/GRAD exist in the core context but the baseline UI has no control for changing the angle mode;
* integral, derivative, equation solving, and table derivative operations are numerical approximations;
* graph discontinuity handling and trace are finite-resolution heuristics; midpoint bridging currently goes two levels deep;
* very closely spaced real polynomial roots (approximately `1e-5 * (1 + |x|)`) may be merged as repeated roots;
* integer ratio output is limited by the signed 64-bit range;
* operation forms still serialize into finite command strings; `MatA..MatD` / `VctA..VctD` exist, but there are no persistent named lists or spreadsheet functionality;
* no spreadsheet, eActivity, program editor, conics, dynamic graphing, CAS, or full Graph Solve functionality;
* some functions with extreme conditions like tan(), csc() will get this calculator fuck up.
## Reference documentation

* [Casio Natural textbook input](https://support.casio.com/global/en/calc/manual/fx-97SGCW_en/inputting_expressions_and_values/inputting_an_expression_using_natural_textbook_format.html)
* [fx-9750GIII Software User’s Guide v3.21](https://support.casio.com/storage/en/manual/pdf/EN/004/fx-9750GIII_Soft_v321_EN.pdf)
* [Casio Graph&Table App](https://support.casio.com/global/en/calc/manual/fx-CG100_1AUGRAPHv210_en/JEAWSYadjpqxtp.html)

These links are used only as behavioral references; the repository’s contract is always the source code and tests in the current checkout.
