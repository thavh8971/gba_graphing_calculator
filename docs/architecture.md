# Kiến trúc

## Phạm vi thiết kế

Dự án này là một implementation C mới cho graphing-calculator prototype trên Game Boy Advance. Mục tiêu kỹ thuật là giữ phần lõi có thể chạy test trên máy host, còn lớp GBA chỉ làm nhiệm vụ đọc phím, đồng bộ VBlank và nối framebuffer Mode 3. Dự án không nhúng source cũ, không cần emulator để build hoặc chạy host test, và không giả định tương thích nhị phân hay tương thích giao diện với máy Casio.

Các ranh giới chính:

- độ phân giải đích là 240×160, màu RGB15, một framebuffer Mode 3 trong VRAM;
- source của dự án không gọi trực tiếp API cấp phát động;
- evaluator, parser, mode dispatcher và graph sampler không phụ thuộc `gba.h`;
- state của một phiên tính nằm trong `CalcContext`/`ModeRuntime`, không bắt buộc dùng singleton;
- các giới hạn bộ nhớ đều là hằng số lúc biên dịch.

## Sơ đồ module

```text
source/platform/gba_main.c
        |
        v
include/gcalc/app.h  <->  source/app/app.c
        |               /       |        \
        v              v        v         v
 ui/gfx.c       ui/natural.c  modes/*   graph/graph.c
                                  \       /
                                   v     v
                              core/calc.c
                                   |
                                   v
                             core/syntax.c

core/decimal.c là arithmetic decimal độc lập; nó chưa nằm trên mọi đường
đánh giá của core/calc.c.
```

Các header public nằm dưới `include/gcalc/`. File nội bộ duy nhất của các mode là `source/modes/mode_internal.h`; code ngoài thư mục này chỉ gọi `modeEvaluate()`.

## Vòng đời trên GBA

`source/platform/gba_main.c` tạo một `AppState` tĩnh trong EWRAM, bật `MODE_3 | BG2_ENABLE`, rồi giữ nguyên chế độ PPU. Mỗi vòng lặp:

1. chờ VBlank;
2. đọc `keysDown()` và `keysHeld()` từ libgba;
3. chuyển chúng sang bitmask độc lập nền tảng `APP_KEY_*`;
4. gọi `appHandleKeys()`;
5. gọi `appTick()` để tiến công việc theo frame;
6. gọi `appRender()` với một `GfxSurface` trỏ thẳng tới `MODE3_FB`.

`GfxSurface` chứa con trỏ pixel, chiều rộng, chiều cao và stride. Vì vậy primitive đồ họa vẫn có thể được test với một buffer RAM trên host; chỉ `gba_main.c` biết địa chỉ VRAM thật.

`AppDirty` chia giao diện thành header, biểu thức, kết quả, status, keypad và viewport. Di chuyển keypad chỉ vẽ lại cell cũ/mới. Đổi page ở keypad của mode hoặc GRAPH vẽ lại keypad, status và header vì các vùng này mang trạng thái trang. Cursor blink và edit chỉ làm bẩn vùng expression/result/status liên quan. Pan/zoom xóa viewport 240×134 rồi restart graph stream; table scroll/tính lại chỉ refresh header và vùng table. Chỉ view/menu switch đặt `APP_DIRTY_ALL` và clear toàn framebuffer. Đây là contract của app; primitive trong `gfx.c` không tự quản lý dirty state.

Mọi keypad được dựng thành lưới 6×5 với action `EXE` cố định ở index 29. Catalogue page được lọc theo mode: COMP 7, CMPLX 8, STAT 3, BASE-N 2, EQN 4, MATRIX 3, TABLE 5, VECTOR 3, INEQ 4, RATIO 2, DIST 3 và GRAPH 4. D-pad vượt bất kỳ mép nào đổi page tuần hoàn và giữ hàng/cột trên trục còn lại.

Các mode có state flow riêng thay vì giả lập toàn bộ bằng calculator editor:

```text
COMP, CMPLX                  -> APP_VIEW_CALCULATOR
STAT                         -> APP_VIEW_STAT_TYPE -> APP_VIEW_STAT_DATA
BASE-N                       -> APP_VIEW_BASEN
EQN, INEQ                    -> APP_VIEW_MODE_ACTION -> APP_VIEW_MODE_GRID
MATRIX, VECTOR               -> APP_VIEW_MODE_ACTION -> APP_VIEW_MODE_GRID (EDIT register)
                                                   -> APP_VIEW_MODE_FORM (f())
TABLE, RATIO, DIST           -> APP_VIEW_MODE_ACTION -> APP_VIEW_MODE_FORM
GRAPHING                     -> APP_VIEW_GRAPH_INPUT -> APP_VIEW_GRAPH
```

Form/grid/table chỉ là representation UI; tại biên engine, form phép toán được serialize thành canonical `name(...)` command. STAT đi qua `modeStatEvaluateRows()` để không cần buffer command lớn cho tối đa 64 hàng. Editor `MatA..MatD`/`VctA..VctD` tính trực tiếp từng ô và ghi `CalcNumber` vào `ModeRuntime`; ô trống được lưu thành 0. Buffer/cursor của calculator, graph, base, form, STAT và grid nằm riêng trong `AppState`, nên đổi mode không làm source hoặc vùng nhớ của editor khác biến đổi.

GRAPH có state flow riêng:

```text
APP_VIEW_GRAPH_INPUT -- EXE+A hoặc START --> APP_VIEW_GRAPH
APP_VIEW_GRAPH       -- B ----------------> APP_VIEW_GRAPH_INPUT
```

`graphExpression`, `graphCursor`, page và keypad selection độc lập với expression editor của COMP. Màn hình GRAPH input dùng keypad 6×5 trên bốn trang; index 29 được xử lý như action `EXE` cố định. D-pad vượt mép trái/phải/trên/dưới đổi trang và giữ tọa độ trên trục còn lại. `menuReturnView` bảo toàn đúng editor hay viewport khi mở rồi hủy mode menu.

## Parser và AST

`calcSyntaxParse()` dùng recursive descent và sinh AST kích thước cố định:

| Giới hạn | Giá trị |
|---|---:|
| Node tối đa | 96 |
| Độ dài `text` trên một node | 63 ký tự + NUL |
| Đối số tối đa của một call | 6 |
| Độ sâu evaluator tối đa | 112 |
| Độ dài expression của app | 127 ký tự + NUL |

Mỗi node ghi `kind`, toán tử, text, child/argument index và source span nửa mở `[start, end)`. Các loại node gồm number, identifier, unary, binary, call, relation và postfix. Source span là dữ liệu cấu trúc dùng được cho editor/renderer; nó không phải offset glyph sau khi layout textbook.

Thứ tự ưu tiên được mã hóa như sau:

```text
relation
  -> sum (+, -)
    -> product (*, /, implicit multiplication)
      -> unary (+, -)
        -> power (^, kết hợp phải qua parseUnary)
          -> postfix (!, %)
            -> primary
```

Parser nhận:

- số thập phân dùng dấu chấm hoặc dấu phẩy;
- scientific notation `E`/`e` với exponent có dấu;
- implicit multiplication như `2x`, `xy`, `2sin(x)` và `2(1+x)`;
- relation `=`, `==`, `!=`, `<`, `<=`, `>` và `>=`;
- inverse-trig spelling `sin^-1`, `cos^-1`, `tan^-1`;
- call nhiều đối số bằng `;`, hoặc bằng `,` khi dấu phẩy không bị hiểu là dấu thập phân.

Để tránh mơ hồ, tài liệu và test dùng `;` cho call nhiều đối số. Ví dụ `atanh(0,5)` là một đối số thập phân 0.5, còn `normalpdf(0,0,1)` là ba đối số vì `normalpdf` là call nhiều đối số.

`calcSyntaxParseGraphRow()` tạo typed row riêng cho `y=`, `x=`, `r=`, canonical `param(...)` (cùng alias `param:` lịch sử) và inequality. Graph parser cho phép biểu thức trần và chuẩn hóa nó thành hàng `y=`.

Evaluator giữ parse cache round-robin bốn slot và fallback AST trong `.sbss` EWRAM. Mỗi entry sở hữu bản copy source tối đa 127 byte và AST immutable; lookup so sánh nội dung chuỗi, không dùng địa chỉ buffer của caller. Vì vậy editor có thể evaluate lặp lại rồi mutate cùng một buffer mà không dùng nhầm AST cũ. Expression dài hơn giới hạn cache được parse vào fallback AST riêng và không cache. Cache là workspace dùng chung, phù hợp với main loop đơn luồng của GBA; evaluator không được gọi đồng thời hoặc từ IRQ.

## Numeric contract

### CalcNumber

Giá trị public của evaluator là:

```c
typedef struct CalcNumber {
    s32 mantissa;
    s16 exponent;
} CalcNumber;
```

Mantissa lưu chín chữ số có nghĩa đã chuẩn hóa; exponent thập phân hợp lệ từ -100 đến 100. Ví dụ về mặt khái niệm, `mantissa = 123456789, exponent = 2` biểu diễn `1.23456789E2`. Kết quả được làm tròn về contract này tại biên `calcNumberFromLongDouble()`.

AST được tính bằng kiểu `long double` mà toolchain cung cấp, sau đó mới lượng tử hóa thành `CalcNumber`. Vì kích thước/độ chính xác của `long double` phụ thuộc ABI, đây không phải engine decimal tùy ý độ chính xác và cũng không phải mô phỏng nội bộ máy Casio. Formatter dùng `%g` trên giá trị đã thu hẹp về `double` để tránh khác biệt ABI `%Lf` của MinGW UCRT; public core vẫn chỉ giữ chín chữ số có nghĩa.

`CalcContext` sở hữu độc lập:

- 26 biến A–Z và bitmask hiện diện;
- `Ans` và `PreAns`;
- angle mode RAD/DEG/GRAD;
- state PRNG.

Các phép graph/table tạm bind X, Y hoặc T rồi phục hồi biến cũ. `calcEvaluateContext()` cập nhật Answer memory cho một lần evaluate trực tiếp; graph sampler snapshot và phục hồi `Ans`/`PreAns` quanh từng sample để việc render không ghi đè memory của người dùng.

Tích phân dùng Simpson với 256 khoảng. Đạo hàm dùng sai phân trung tâm với bước phụ thuộc điểm đánh giá. Tổng dùng compensated summation và tích dùng phép nhân tuần tự; series bị chặn ở 100.000 lần lặp. Những thuật toán này là số, không phải symbolic calculus.

### DecimalNumber

`DecimalNumber` lưu tối đa 32 chữ số thập phân và cung cấp parse, cộng, trừ, nhân, chia dài và format có rounding. Workspace nội bộ lớn hơn được cấp trên stack trong từng phép toán. Module này có test riêng nhưng chưa thay thế `CalcNumber` trong evaluator tổng quát. Không được suy ra độ chính xác 32 chữ số cho COMP, graph, mode solver hoặc distribution.

### Fixed-point của graph

Tọa độ graph dùng `CALC_ONE = 256`, tức Q24.8 trong `s32`. Chuyển đổi từ `CalcNumber` có kiểm tra range; một giá trị hợp lệ về toán học vẫn có thể trở thành `GRAPH_SAMPLE_OVERFLOW` nếu không biểu diễn được trong fixed-point.

## Mode runtime

`ModeRuntime` gom `CalcContext`, bảng tối đa 16 hàng, bốn matrix register 4×4, bốn vector register 3D, result buffer 160 byte và mã lỗi. `modeEvaluate()` xóa result/table tạm rồi dispatch tới một trong 12 mode nhưng giữ nguyên `MatA..MatD`/`VctA..VctD`. Runtime chưa có object database cho list, spreadsheet hoặc program; phép toán từ form đi vào engine bằng canonical command string.

Các mode dùng chung helper để:

- so khớp command prefix không phân biệt hoa/thường;
- tách field ở top level, không cắt bên trong `()` hoặc `[]`;
- evaluate mỗi field bằng core evaluator;
- format real result có loại bỏ số 0 cuối.

Một số mode dùng `long double` trực tiếp cho thuật toán chuyên biệt (complex, matrix, regression, distribution). Kết quả của các đường này không nhất thiết trải qua `CalcNumber` lần cuối; input subexpression vẫn đi qua evaluator khi mode gọi `modeEvalReal()`.

## Graph contract

Graph core tách parse, sample và clip khỏi rasterizer.

### Function list

`graphParseFunctions()` nhận tối đa 10 hàng. Dấu `;` top-level phân cách hàng; trong canonical `param(x(t);y(t))` separator nằm trong ngoặc, còn parser vẫn nhận ngoại lệ `param:x(t);y(t)` cho save cũ. Mỗi `GraphFunction` giữ source gốc, typed row, cờ enabled/valid và phase của call `tan(...)` đầu tiên tìm thấy.

Các dạng row:

```text
y=f(x)                hoặc biểu thức trần f(x)
x=f(y)
r=f(t)
param(x(t);y(t))
y<f(x), y<=f(x), y>f(x), y>=f(x)
x<f(y), x<=f(y), x>f(y), x>=f(y)
```

Inequality row hiện cung cấp boundary sample và relation metadata. Việc tô đúng miền là trách nhiệm renderer; đây không phải symbolic inequality engine hai biến.

### Streaming

`GraphJob` chỉ giữ function index, sample index và sample trước đó. `graphJobStep()` phát tối đa số phần tử mà caller cấp; API công bố capacity chunk 40, còn app renderer chủ động lấy 32 sample mỗi frame. Không có mảng lưu toàn bộ đường cong. App khởi tạo 240 sample cho mỗi function và viewport plot 240×134.

- `y=` và inequality theo Y lấy parameter từ x-range;
- `x=` và inequality theo X lấy parameter từ y-range;
- polar và parametric lấy đúng một vòng theo angle mode: 2π, 360 hoặc 400;
- mỗi sample mang `VALID`, `DOMAIN_ERROR`, `POLE` hoặc `OVERFLOW`, cùng `breakBefore`.

Khi tìm thấy `tan(g(...))`, sampler còn đánh giá phase `g(...)` ngay cả khi cần theo dõi pole. Crossing tại π/2 + kπ tạo `POLE` và cắt segment. Đây là detector chuyên biệt cho tangent, không phải singularity analysis tổng quát.

### Clip và discontinuity

Segment hợp lệ được clip trong world coordinates bằng Cohen–Sutherland rồi mới đổi sang pixel. Domain error, overflow, pole hoặc sample không liên tiếp làm đứt branch.

Đối với biểu thức tùy ý, sampling hữu hạn chỉ là heuristic. Một tiệm cận không rơi đúng sample và không nằm trong detector tangent vẫn có thể bị nối nhầm; một đặc trưng hẹp có thể bị bỏ qua. Core hiện không phân tích mẫu số, domain hay singularity theo symbolic algebra.

## Natural editor

Editor giữ một chuỗi tuyến tính duy nhất để parser xử lý. `NaturalCursor` bổ sung `node`, `slot` và byte offset. Mỗi lần di chuyển/chèn/xóa, module dựng lại tối đa 64 cấu trúc tạm để nhận diện:

- numerator/denominator của `/`;
- base/exponent của `^`;
- radicand và root index;
- body, variable, lower/upper bound của `sum`, `prod`, `integral`;
- body, variable và evaluation point của derivative.

Di chuyển ngang thay đổi offset một byte. Di chuyển dọc giữ cột tương đối và chuyển sang slot kế cận trong cùng template. Đây là structural overlay trên text, không phải cây editable độc lập; edit chưa có undo/redo và không giữ placeholder object như firmware máy tính thật.

## Đồ họa và RAM

Mode 3 dùng 240 × 160 × 2 = 76.800 byte VRAM. Dự án không tạo shadow framebuffer trong EWRAM. UI app dùng đủ 95 glyph ASCII in được với bitmap 5×7, advance 6 pixel và line advance 8 pixel cho mọi mode: header, Natural expression, result, status, mode menu, table, keypad và GRAPH. API/font 5×9 raster transparent vẫn tồn tại trong `gfx.c` để tương thích nhưng không còn được gọi từ `app.c`.

`gfxClear()` và từng scanline của `gfxFillRect()` đi qua span-fill. Host build dùng C fallback để test clipping/stride; GBA build gọi `gcalcArmFill16` viết bằng ARM assembly. Symbol được đặt trong section `.iwram`; target `verify-arm-fast` đọc ELF symbol table để bắt cả địa chỉ IWRAM `0x03000000..0x03007fff` và ARM-state bit chẵn.

Chiến lược RAM hiện tại:

- `AppState` là `EWRAM_BSS`, không nằm trên stack IWRAM;
- graph giữ tối đa 10 mô tả function và stream sample thay vì cache cả plot;
- expression, result và table có capacity cố định;
- source không gọi `malloc()`/`free()`, nhưng `snprintf`/`strtold` của newlib kéo allocator symbols vào ELF; vì vậy không được coi artifact hiện tại là hard no-heap runtime;
- parse cache và fallback AST dùng `.sbss` EWRAM được CRT zero trước `main`; một số workspace graph/decimal/mode vẫn là vùng cục bộ tương đối lớn cần theo dõi bằng map và memory-budget test.

Target `verify-gba-memory` kiểm tra cache vẫn ở EWRAM, fast-fill vẫn là ARM code trong IWRAM và khoảng cách từ `__data_end__` tới `__sp_usr` còn ít nhất 18 KiB. Host test vẫn không chứng minh timing VBlank, cartridge boot hay hình ảnh cuối trên phần cứng.

## Build và test boundary

Makefile tạo hai graph build độc lập:

- GBA: mọi source portable cộng `source/platform/gba_main.c`, compile bằng devkitARM, link libgba và libm;
- host: mọi source portable, không gồm platform GBA, link từng `tests/*_test.c` thành binary riêng với host GCC và libm.

Dependency file `.d` được sinh cho cả hai. `--gc-sections` loại section không dùng. ROM được `gbafix` với metadata cố định rồi `verify-rom-header` đọc lại byte thực trong ROM; `verify-gba-memory` kiểm tra ARM/IWRAM fast-fill, EWRAM cache và stack margin sau link.

Host tests chứng minh contract logic trong môi trường host; chúng không chứng minh ABI `long double` trên ARM, hiệu năng mỗi frame, cartridge boot hay hình ảnh cuối. Việc verify tích hợp phải ghi riêng size, SHA-256 và kết quả test sinh ra tại lần chạy đó.
