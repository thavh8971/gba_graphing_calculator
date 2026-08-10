# GBA Graphing Calculator


<img width="476" height="374" alt="image" src="https://github.com/user-attachments/assets/c424f61c-3c42-4b79-b6dc-3f1e75a4f4ef" />

<img width="475" height="366" alt="image" src="https://github.com/user-attachments/assets/a88061b8-4b49-4922-b858-6b5dec00b4a3" />

<img width="476" height="370" alt="image" src="https://github.com/user-attachments/assets/df4e6769-0bc8-4454-84f1-c9b95d835e5e" />

<img width="473" height="367" alt="image" src="https://github.com/user-attachments/assets/b0b6cd2a-ed8a-49b1-98d1-396fc9ccdbe6" />


Một graphing-calculator prototype viết lại theo kiến trúc module cho Game Boy Advance. ROM đích chạy ở 240×160, Mode 3, vẽ RGB15 trực tiếp vào framebuffer VRAM. Parser, evaluator, các mode và graph sampler đều build được trên host mà không cần libgba; chỉ lớp platform chạm vào phần cứng GBA.

Đây là bản thử nghiệm chức năng, không phải firmware Casio và không tuyên bố clone đầy đủ bất kỳ model nào. Dự án cũng không tuyên bố có 453 function/command.

## Có gì trong repository

```text
include/gcalc/            public C API
source/core/              parser-independent number/evaluator/decimal core
source/modes/             dispatcher và implementation từng mode
source/graph/             typed graph row, streaming sampler, world clip
source/ui/                framebuffer primitives, font, Natural cursor
source/app/               state machine và renderer dùng chung
source/platform/          GBA entry point + libgba key/VBlank bridge
tests/                    host test executables
scripts/build.ps1         Windows build/verify entry point
docs/                     contract, architecture và toolchain snapshot
```

Chi tiết thiết kế nằm ở [docs/architecture.md](docs/architecture.md). Cú pháp đầy đủ và ranh giới so với tài liệu Casio nằm ở [docs/reference_matrix.md](docs/reference_matrix.md).

## Yêu cầu build

Trên Windows:

- PowerShell 5.1 hoặc mới hơn;
- GNU Make và shell/utilities kiểu MSYS2;
- host GCC để chạy host tests;
- devkitARM, libgba và `gbafix` từ devkitPro để build ROM.

`scripts/build.ps1` tự dò `C:\devkitPro` và `C:\msys64\opt\devkitpro`; có thể override bằng tham số. Snapshot package tham chiếu được ghi ở [docs/toolchain.lock.md](docs/toolchain.lock.md). Toolchain binary không được vendor trong repository.

## Build và verify

Chạy từ PowerShell tại root dự án:

```powershell
# Build và chạy mọi host test
.\scripts\build.ps1 host-test

# Build ROM
.\scripts\build.ps1 gba

# Rebuild host tests + ROM, rồi kiểm tra cartridge header thực
.\scripts\build.ps1 verify -Rebuild

# In metadata, kích thước và SHA-256 của ROM vừa build
.\scripts\build.ps1 rom-info
```

Nếu toolchain ở vị trí khác:

```powershell
.\scripts\build.ps1 verify -Rebuild `
  -DevkitPro C:\msys64\opt\devkitpro `
  -Make C:\msys64\usr\bin\make.exe
```

Artifacts được sinh dưới:

```text
gba_graphing_calculator.gba                 ROM phát hành để mở/chạy
build/gba/gba_graphing_calculator.gba
build/gba/gba_graphing_calculator.elf
build/gba/gba_graphing_calculator.map
build/host/*_test.exe
```

ROM ở project root luôn được copy byte-for-byte từ `build/gba` trong target `gba`, `verify` và `rom-info`; đây là đường dẫn canonical để tránh mở nhầm một ROM cũ.

README này cố ý không chép size, checksum hoặc chữ “PASS” từ project cũ. Giá trị bàn giao phải lấy từ `verify`/`rom-info` trên chính checkout hiện tại.

## Điều khiển

### COMP/CMPLX expression editor

| Phím | Hành vi |
|---|---|
| D-pad | Di chuyển selection; đi vượt mép lưới sẽ đổi trang và giữ hàng/cột tương ứng |
| A | Chèn token; nếu đang chọn `EXE` thì execute mode hiện tại |
| B | Xóa ký tự ngay trước cursor |
| L / R | Di chuyển cursor biểu thức sang trái / phải |
| SELECT + UP / DOWN | Chuyển structural slot theo chiều dọc trong fraction, power, root hoặc calculus template |
| B + LEFT / RIGHT | Chuyển trang keypad |
| START | Shortcut tương đương chọn `EXE` rồi nhấn A |
| SELECT + START | Mở/đóng mode menu |

Editor vẫn lưu biểu thức dạng text tuyến tính. Natural cursor nhận diện numerator, denominator, base, exponent, root index/radicand, body/variable/bounds và điểm đạo hàm. Vì vậy parser luôn nhận cùng một source string, còn UI có thể layout textbook mà không duy trì hai biểu diễn toán học khác nhau.

Mọi keypad là lưới 6×5 với `EXE` cố định ở ô 29. Page được lọc theo mode thay vì dùng chung toàn bộ catalogue: COMP 7 page và CMPLX 8 page; các workspace khác có 2–5 page phù hợp với dữ liệu của chúng. D-pad vượt mép lưới đổi page tuần hoàn và giữ hàng/cột trên trục còn lại.

Toàn bộ chữ do app render—header, expression, kết quả, status, mode menu, table, keypad và GRAPH—dùng glyph printable ASCII 5×7. API 5×9 cũ chỉ còn được giữ để tương thích ở tầng đồ họa, không còn được gọi từ UI app.

### Mode menu

| Phím | Hành vi |
|---|---|
| D-pad | Chọn một trong 12 mode trên lưới 3×4 |
| A / START | Xác nhận mode |
| B | Đóng menu, giữ mode trước đó |
| SELECT + START | Đóng menu |

Chọn mode 12 mở màn hình nhập biểu thức GRAPH riêng. Biểu thức GRAPH được giữ trong buffer riêng với COMP và vẫn còn nguyên khi chuyển qua lại giữa editor và viewport.

### Workspace theo mode

- STAT mở selector 8 loại, sau đó là bảng X/Freq cho 1-VAR hoặc X/Y/Freq cho regression. `SELECT+D-pad` đổi cell, `EXE` sang cell kế và `START` tính.
- BASE-N có buffer riêng, bốn lựa chọn DEC/HEX/BIN/OCT, digit gating theo radix và hai page arithmetic/logic. `Neg` nhập bù hai 32-bit.
- EQN và INEQ mở action selector rồi grid hệ số. MATRIX/VECTOR có bốn vùng nhớ `MatA..MatD`/`VctA..VctD`: chọn `EDIT` để mở bảng textbook với một cặp ngoặc vuông lớn, `SELECT+D-pad` đổi ô, `SELECT+L/R` đổi dimension, `SELECT+B` xóa và `B` hoặc `START` lưu rồi thoát. Các phép toán ma trận/vector mở form `f()` đã điền sẵn tên vùng nhớ.
- TABLE, RATIO và DIST mở action selector rồi form. `SELECT+UP/DOWN` đổi field, `START` hoặc `EXE` tính; `B` trên field rỗng quay lại selector.

Các workspace giữ buffer riêng với COMP và GRAPH. Form/grid phép toán serialize thành canonical command string ở biên mode engine; riêng STAT gửi mảng hàng trực tiếp và editor `Mat`/`Vct` tính từng ô rồi ghi thẳng vào vùng nhớ cố định để không phụ thuộc một command buffer lớn.

### GRAPH expression input

| Phím | Hành vi |
|---|---|
| D-pad | Di chuyển selection; đi vượt mép lưới sẽ sang trang trước/sau và giữ hàng hoặc cột tương ứng |
| A | Chèn token; nếu đang chọn ô `EXE` thì parse và vẽ biểu thức hiện tại |
| B | Xóa ký tự ngay trước cursor |
| L / R | Di chuyển cursor biểu thức sang trái / phải |
| SELECT + UP / DOWN | Chuyển structural slot theo chiều dọc |
| B + LEFT / RIGHT | Chuyển trang trực tiếp |
| START | Shortcut tương đương chọn `EXE` rồi nhấn A |
| SELECT + START | Mở mode menu |

Keypad GRAPH có 4 trang `PLOT`, `FUNC`, `CALC`, `SYM`. Mỗi trang là lưới 6×5; ô dưới cùng bên phải (index 29) luôn là `EXE` và không bao giờ chèn chữ vào biểu thức. Toàn bộ ký tự ở GRAPH input, graph header và graph status dùng glyph 5×7.

### Graph view

| Phím | Hành vi |
|---|---|
| START | Parse lại function list và bắt đầu plot mới |
| D-pad khi trace tắt | Pan viewport |
| L / R | Zoom out / in |
| A | Bật/tắt trace |
| LEFT / RIGHT khi trace bật | Di chuyển trace theo sample |
| UP / DOWN khi trace bật | Chuyển function đang trace |
| B | Quay về GRAPH expression input và giữ nguyên biểu thức/cursor |
| SELECT + START | Mở mode menu |

Viewport mặc định có tâm (0,0), `x=[-10,10]`; vùng plot 240×134 cho `y` xấp xỉ `[-5.58,5.58]`. Mỗi function dùng 240 sample và renderer lấy tối đa 32 sample mỗi frame, nên UI không đợi cả function list xong mới tiếp tục vòng lặp VBlank.

### Table view

| Phím | Hành vi |
|---|---|
| UP / DOWN | Scroll các row khi bảng dài hơn vùng hiển thị |
| START | Tính lại bảng từ expression hiện tại |
| B | Quay về TABLE form nếu bảng được tạo từ form; nếu không thì về expression editor |
| SELECT + START | Mở mode menu |

## Biểu thức

Grammar chính nhận:

```text
+ - * / ^
! %
( )
= == != < <= > >=
2x  xy  2sin(x)  2(1+x)
1.25  1,25  1E100  1E-100
```

Ví dụ:

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

Dùng `;` để phân cách đối số nhiều ngôi. Dấu phẩy còn có vai trò dấu thập phân, nên `;` rõ nghĩa hơn trong input thủ công.

Kết quả evaluator public dùng `CalcNumber`: chín chữ số có nghĩa, exponent thập phân -100..100. `DecimalNumber` 32 chữ số là module riêng và chưa thay thế evaluator tổng quát. Context mặc định là RAD; core API có DEG/GRAD nhưng UI baseline chưa gán phím đổi angle mode.

## Mười hai mode

Mode menu:

```text
1: COMP      2: CMPLX     3: STAT       4: BASE-N
5: EQN       6: MATRIX    7: TABLE      8: VECTOR
9: INEQ     10: RATIO    11: DIST      12: GRAPHING
```

Engine nhận compact command syntax; UI cung cấp editor riêng theo mode và serialize về cú pháp này:

| Mode | Ví dụ contract |
|---|---|
| COMP | `sum(x^2;x;1;3)`, `ncr(10;3)`, `normalpdf(0;0;1)` |
| CMPLX | `3+4i`, `conj:3+4i`, `pow:1+i;2`, `polar:2;pi/4` |
| STAT | `1;2;3;4`, `170,66;173,68;179,75`, `freq:1,2,3;2,1,1` |
| BASE-N | `bin(1011)`, `hex(FF)`, `101+1`, `xnor(15;7)`, `neg(1)` |
| EQN | `solve:x^2=2;1`, `solven:x^2=1;-2;2`, `lin:1,1,3;2,-1,0`, `poly:1;-3;2` |
| MATRIX | `det(MatA)`, `inv(MatA)`, `transpose(MatA)`, `mul(MatA;MatB)` |
| TABLE | `x^2;0;5;1`, `x;x^2;0;5;1`, `dtable:x^2;0;5;1` |
| VECTOR | `norm(VctA)`, `dot(VctA;VctB)`, `cross(VctA;VctB)`, `scale(2;VctA)` |
| INEQ | `2*x<4`, `ineq2(1;0;-4;<)`, `ineq3(1;0;0;0;<)`, `ineq4(1;0;0;0;-1;<)` |
| RATIO | `1.5:2.5`, `2:3=8:?` |
| DIST | `binomcdf(5;10;0.5)`, `normcdf(-1;1;1;0)`, `norminv(0.5;1;0)` |
| GRAPHING | `x^2;sin(x)`, `param(cos(t);sin(t))`, `x>=y^2` |

Danh sách alias, giới hạn shape/range và thứ tự đối số chuẩn nằm trong [docs/reference_matrix.md](docs/reference_matrix.md).

## Graph pipeline

Function list nhận tối đa 10 row:

```text
y=f(x)                 hoặc f(x)
x=f(y)
r=f(t)
param(x(t);y(t))       (`param:x(t);y(t)` là compatibility alias)
y<f(x), y<=f(x), y>f(x), y>=f(x)
x<f(y), x<=f(y), x>f(y), x>=f(y)
```

Sampler phát `GraphSample` theo chunk, kèm fixed-point coordinates, parameter, trạng thái domain/pole/overflow và `breakBefore`. Renderer clip segment ở world coordinates trước khi rasterize. Với `tan(g(x))`, phase crossing tại π/2 + kπ được theo dõi riêng để tránh nối qua pole thường gặp.

Đây vẫn là sampling hữu hạn. Discontinuity tùy ý được xử lý bằng state/detector/heuristic, không phải symbolic singularity analysis; một tiệm cận hoặc chi tiết rất hẹp vẫn có thể bị nối nhầm hoặc bỏ sót.

## RAM và framebuffer

- framebuffer Mode 3 chiếm 76.800 byte VRAM;
- không có shadow framebuffer đầy màn hình trong EWRAM;
- `AppState`, parse cache và fallback AST tĩnh được đặt trong EWRAM;
- graph stream tối đa 32 sample mỗi frame, không giữ toàn bộ plot;
- expression, AST, table, function list và result đều có capacity cố định;
- source của dự án không gọi trực tiếp `malloc`/`free`; tuy nhiên artifact ARM hiện liên kết `snprintf`/`strtold` của newlib và kéo theo allocator/heap support, nên đây không phải hard no-heap build.

Một số workspace graph/mode/decimal là biến cục bộ tương đối lớn. `verify-gba-memory` bắt cache sai section, ARM fast-fill sai vùng/state và stack margin dưới 18 KiB; host tests vẫn không bắt được lỗi timing/VRAM hay hình ảnh thực trên GBA.

## Verification boundary

Các suite host hiện có nhằm kiểm tra:

- parser/evaluator, context isolation và AST cache khi lặp/mutate input;
- decimal arithmetic và Natural cursor;
- route/page/keypad của từng workspace, form/grid, STAT, BASE-N và GRAPH EXE/B;
- mode runtime/alias, graph row classification, streaming, clipping và framebuffer stride/guard.

`verify-rom-header` còn đọc lại title/game code/maker/version trong ROM sau `gbafix`. Không bước nào ở trên chứng minh hình ảnh hoặc input trên emulator/phần cứng. Baseline tài liệu này chưa có visual emulator verification và quá trình phát triển không dùng Computer Use/emulator.

## Giới hạn hiện tại

- chưa phải clone đầy đủ fx-9750GII/GIII, fx-570VN PLUS hay model Casio khác;
- không có catalogue 453 function/command;
- `DecimalNumber` chưa là backend chung;
- DEG/GRAD có trong core context nhưng chưa có control đổi angle mode trên UI baseline;
- integral, derivative, equation solve và table derivative là phép tính số;
- graph discontinuity và trace là heuristic hữu hạn độ phân giải; midpoint bridge hiện sâu 2 mức;
- nghiệm đa thức thực rất gần nhau (xấp xỉ `1e-5 * (1 + |x|)`) có thể bị gộp như một nghiệm lặp;
- output tỷ số nguyên bị giới hạn bởi miền signed 64-bit;
- form phép toán vẫn serialize về command string hữu hạn; đã có `MatA..MatD`/`VctA..VctD`, nhưng chưa có persistent named list hoặc spreadsheet;
- chưa có spreadsheet, eActivity, program editor, conics, dynamic graph, CAS hoặc Graph Solve đầy đủ;
- chưa có emulator/hardware visual verification trong baseline này.

## Tài liệu tham chiếu

- [Casio Natural textbook input](https://support.casio.com/global/en/calc/manual/fx-97SGCW_en/inputting_expressions_and_values/inputting_an_expression_using_natural_textbook_format.html)
- [fx-9750GIII Software User’s Guide v3.21](https://support.casio.com/storage/en/manual/pdf/EN/004/fx-9750GIII_Soft_v321_EN.pdf)
- [Casio Graph&Table App](https://support.casio.com/global/en/calc/manual/fx-CG100_1AUGRAPHv210_en/JEAWSYadjpqxtp.html)

Các link chỉ dùng làm behavioral reference; contract của repository luôn là source + tests trong checkout hiện tại.
