# Ma trận contract và tham chiếu

Tài liệu này mô tả những gì source hiện thực sự nhận, không phải danh sách tính năng mong muốn. Kết quả pass/fail, kích thước ROM và checksum chỉ có giá trị khi được sinh trong một lần `scripts/build.ps1 verify`/`rom-info`; chúng không được suy ra từ ma trận này.

## Nguồn Casio dùng để định hướng

Các nguồn dưới đây đều là tài liệu chính thức của Casio và đã được kiểm tra đường dẫn ngày 2026-08-08:

1. [Inputting an Expression Using Natural Textbook Format](https://support.casio.com/global/en/calc/manual/fx-97SGCW_en/inputting_expressions_and_values/inputting_an_expression_using_natural_textbook_format.html) — mô tả template có vùng nhập riêng và cách cursor đi vào/ra vùng.
2. [fx-9750GIII / fx-9860GIII Software User’s Guide, version 3.21 (PDF)](https://support.casio.com/storage/en/manual/pdf/EN/004/fx-9750GIII_Soft_v321_EN.pdf) — tham chiếu tên và workflow của các nhóm RUN-MAT, STAT, EQUA, TABLE và GRAPH trên dòng graphing calculator.
3. [Graph&Table App Software User’s Guide](https://support.casio.com/global/en/calc/manual/fx-CG100_1AUGRAPHv210_en/JEAWSYadjpqxtp.html) — tham chiếu các loại `y=`, `r=`, parametric, `X=`, inequality, function list, table, view window, pan/zoom và trace/analysis workflow.

Đây là nguồn tham khảo hành vi cấp cao. Dự án không dùng firmware, ROM, asset, font hay source của Casio; tên thương hiệu không biểu thị chứng nhận hoặc tương thích hoàn toàn.

## UI contract dùng chung

- editor của toàn bộ mode dùng glyph 5×7;
- mọi editor keypad dùng lưới 6×5, mỗi page có 29 token/action và `EXE` cố định ở index 29;
- page được cô lập theo mode: COMP 7, CMPLX 8, STAT 3, BASE-N 2, EQN 4, MATRIX 3, TABLE 5, VECTOR 3, INEQ 4, RATIO 2, DIST 3 và GRAPH 4;
- D-pad vượt mép trái/phải/trên/dưới đổi page và giữ tọa độ trên trục còn lại;
- COMP/CMPLX dùng editor biểu thức; STAT dùng selector 8 loại rồi bảng X/Freq hoặc X/Y/Freq; BASE-N có editor/radix riêng; EQN/INEQ dùng grid; MATRIX/VECTOR dùng grid ngoặc lớn cho `EDIT` register và form cho phép toán `f()`; TABLE/RATIO/DIST dùng action selector rồi form;
- trong editor biểu thức/form, `A` tại `EXE` thực thi; trong bảng/grid, `EXE` chuyển cell. `B`/`START` lưu và thoát editor `Mat`/`Vct`, còn `SELECT+B` xóa trong ô;
- GRAPH dùng editor 6×5 riêng, sau `EXE` mở viewport và `B` quay lại editor mà không mất source.

## Đối chiếu thiết kế

| Chủ đề | Ý tưởng trong tài liệu tham chiếu | Contract của prototype | Khác biệt có chủ ý/hiện tại |
|---|---|---|---|
| Natural input | Template fraction/root/special function có vùng nhập | Buffer text tuyến tính cộng structural slot; cursor có thể đi ngang và chuyển slot dọc | Không có placeholder object, selection màu xám/đen, undo/redo hoặc layout engine tương đương firmware |
| Mode launcher | Ứng dụng tính, thống kê, phương trình, bảng và graph tách theo workflow | Menu 12 mục dispatch tới view riêng: calculator, selector, form, grid, table hoặc graph; MATRIX/VECTOR có bốn register riêng | Form/grid là UI chuyên biệt của prototype nhưng không mô phỏng đầy đủ soft-menu của máy thật |
| Function list | Graph&Table lưu nhiều function với loại row | Tối đa 10 row, phân cách bằng `;` | Tài liệu Graph&Table hiện hành nêu tới 20 function; prototype chỉ giữ 10 |
| Graph type | Cartesian, polar, parametric, inverse Cartesian, inequality | `y=`, `x=`, `r=`, `param(...)` (và alias `param:`), inequality theo X/Y | Không có toàn bộ setup, line style, background, conics hoặc dynamic graph |
| View window | Range, pan, zoom và parameter domain | Viewport fixed-point; app có pan/zoom và sampler theo frame | Polar/parametric hiện lấy một vòng mặc định, chưa có editor Tmin/Tmax/Tpitch |
| Table | Một/hai function và cột derivative tùy chọn | Một/hai function, tối đa 16 row; `dtable:` cho sai phân trung tâm | Không có table spreadsheet, sửa từng cell hay table-to-graph workflow đầy đủ |
| Graph analysis | Trace, root, extrema, intersection, integral | Core có sample metadata; app có trace cơ bản nếu view bật | Không có bộ Graph Solve đầy đủ |

## Contract biểu thức COMP

### Grammar

| Nhóm | Cú pháp |
|---|---|
| Arithmetic | `+ - * / ^`, ngoặc, unary `+/-` |
| Postfix | `!`, `%` |
| Relation trả 0/1 | `=`, `==`, `!=`, `<`, `<=`, `>`, `>=` |
| Implicit multiply | `2x`, `xy`, `2sin(x)`, `2(1+x)` |
| Number | `1.25`, `1,25`, `1E100`, `1E-100` |
| Call separator | ưu tiên `;`; `,` được nhận khi không mơ hồ với dấu thập phân |
| Memories/constants | `Ans`, `PreAns`, `Ran#`, `pi`, `e`, A–Z |

Biến chưa được gán trả 0. API C có thể gán A–Z, graph/table bind X/Y/T tạm thời; UI hiện không có ngôn ngữ assignment tổng quát.

`CalcContext` mặc định dùng RAD. DEG/GRAD là public core contract và được host test trực tiếp, nhưng app baseline chưa expose thao tác đổi angle mode.

### Function catalogue hiện thực

| Nhóm | Tên/alias |
|---|---|
| Trig | `sin`, `cos`, `tan`, `asin`/`sin^-1`, `acos`/`cos^-1`, `atan`/`tan^-1`, `sec`, `csc`, `cot` |
| Hyperbolic | `sinh`, `cosh`, `tanh`, `asinh`, `acosh`, `atanh`, `sech`, `csch`, `coth` |
| Log/power/root | `sqrt`, `cbrt`, `ln`, `log`/`log10`/`lg`, `logab(value;base)`, `exp`, `pow10`, `pow`, `root`/`nroot` |
| Integer/combinatoric | `fac`, `npr`, `ncr`, `gcd`, `lcm`, `mod`/`rmdr`, `intdiv` |
| Real helpers | `min`, `max`, `hypot`, `abs`/`norm`, `recip`, `sqr`, `sign`, `floor`/`intg`, `ceil`, `trunc`/`int`, `frac`, `round` |
| Angle conversion | `deg`, `rad`, `grad`; trig/inverse trig dùng angle mode của context |
| Random | identifier `Ran#`; `RanInt`/`RanInt#(low;high)` |
| Numerical series | `sum`, `prod`/`product`, `integral`/`integrate`, `d/dx`/`derivative`/`diff`, `d2/dx2`/`derivative2` |
| Distribution | `normalpdf`, `normalcdf`, `normalinv`, `binompdf`, `binomcdf`, `poissonpdf`, `poissoncdf`, `geometricpdf`, `geometriccdf`, `hypergeometric`/`hypergeompdf` |

Các template số chấp nhận cả dạng có biến và dạng mặc định X:

```text
sum(body;variable;lower;upper)       sum(body;lower;upper)
prod(body;variable;lower;upper)      prod(body;lower;upper)
integral(body;variable;lower;upper)  integral(body;lower;upper)
d/dx(body;variable;point)            d/dx(body;point)
d2/dx2(body;variable;point)          d2/dx2(body;point)
```

## Command syntax theo mode

Prefix command không phân biệt hoa/thường. Dấu `;` tách record/đối số cấp cao; dấu `,` thường tách cột hoặc thành phần.

### 1: COMP

Nhận trực tiếp grammar và function catalogue ở trên.

```text
2A+3(4+1)
sum(x^2;x;1;3)
integral(sin(x);x;0;pi)
```

### 2: CMPLX

Complex parser riêng nhận số dạng `a+bi`, ngoặc, `+ - * /`, và lũy thừa nguyên từ -64 đến 64.

```text
3+4i                         arithmetic trực tiếp
conj:3+4i
re:3+4i                     im:3+4i
abs:3+4i                    norm:3+4i
arg:3+4i
polar:radius;angle          đổi polar sang a+bi
rect:real;imaginary         trả R=... TH=...
pow:complex;integer
```

`polar:` và `arg:`/`rect:` dùng angle mode hiện tại. Complex mode không có transcendental function phức.

### 3: STAT

```text
v1;v2;v3                    N, mean, population SD
x1,y1;x2,y2;...             linear regression A + Bx và R
freq:v1,v2,...;f1,f2,...    weighted mean/population SD
stat1var(v1;v2;...)         1-variable
linear(x1,y1;...)           A + Bx
quadratic(x1,y1;...)        A + Bx + Cx^2
logarithmic(x1,y1;...)      A + B ln(x)
exponential(x1,y1;...)      A e^(Bx)
abexp(x1,y1;...)            A B^x
power(x1,y1;...)            A x^B
inverse(x1,y1;...)          A + B/x
sum:list                    prod:list
mean:list                   sumx2:list
min:list                    max:list
median:list
varp:list                   vars:list / varsamp:list
sdpop:list                  sdsamp:list
cuml:list
```

Mỗi list/bảng tối đa 64 hàng. UI selector có đủ tám model trên; 1-VAR dùng cột X/Freq, bảy regression dùng X/Y/Freq. Freq bỏ trống tương đương 1, phải là số nguyên không âm và tổng frequency phải dương. Regression cần đủ dữ liệu và variance phù hợp với model.

### 4: BASE-N

```text
2:1011                      prefix legacy cho BIN
8:17
10:-42
16:FF
bin(1011)                   oct(17), dec(-42), hex(FF)
101+1                       arithmetic theo radix đang chọn
(10+1)*11                   precedence và ngoặc
not:value
and:left;right              or:left;right
xor:left;right              xnor:left;right
neg:value                   bù hai 32-bit (phím `Neg`)
shl:value;count             shr:value;count
```

Radix được hỗ trợ là BIN/OCT/DEC/HEX. Input trần và arithmetic `+ - * /` dùng radix đang chọn; `bin(...)`/`oct(...)`/`dec(...)`/`hex(...)` có thể đặt radix rõ ràng cho subexpression. Dạng colon lịch sử của bit command vẫn đọc operand ở hệ 10 để không làm hỏng save cũ. Arithmetic overflow signed 32-bit trả Range ERROR, chia 0 trả `Divide by zero`. Bit operation, `xnor` và `Neg` dùng toàn bộ 32 bit; output luôn gồm DEC/B/O/H trên cùng bit pattern. Số âm ngoài DEC được nhập bằng `Neg`, không bằng dấu trừ; shift count phải nằm trong 0..31.

### 5: EQN

```text
left=right                  Newton, seed mặc định 0
solve:left=right;seed       một nghiệm gần seed
solven:left=right;min;max   quét khoảng rồi bisect đổi dấu
lin:a11,a12,b1;a21,a22,b2  hệ tuyến tính 2..4 ẩn
poly:an;...;a1;a0           hệ số bậc 2..6
```

`poly:` chỉ trả nghiệm thực. Các root candidate cách nhau khoảng
`1e-5 * (1 + |x|)` có thể bị gộp để ổn định nghiệm lặp trong giới hạn chín
chữ số của engine. `solven:` chia khoảng thành 512 đoạn, giữ tối đa 16 nghiệm
phát hiện được và có thể bỏ sót nghiệm tiếp xúc hoặc nghiệm hẹp không đổi dấu.

### 6: MATRIX

Matrix tối đa 4×4. Mode có bốn vùng nhớ cố định `MatA`, `MatB`, `MatC`, `MatD`. Register chưa định nghĩa mở mặc định 4×4; editor vẽ một cặp ngoặc vuông lớn quanh toàn khối, hiển thị ô trống như 0, dùng `B`/`START` để lưu rồi thoát và `SELECT+B` để xóa trong ô. `[]` ngoài cùng của toán hạng inline là tùy chọn; hàng tách bằng `;`, cột bằng `,`.

```text
det(MatA)
inv(MatA)
transpose(MatA)
add(MatA;MatB)
sub(MatA;MatB)
mul(MatA;MatB)
```

Action selector hiển thị tên `f()`, form được điền sẵn `MatA`/`MatB`, và keypad riêng có cả bốn tên register. Determinant/inverse cần matrix vuông; inverse singular trả domain error. Add/sub cần cùng shape; multiply cần inner dimension khớp. Toán hạng inline và alias `name:...` cũ vẫn được engine nhận để tương thích.

### 7: TABLE

```text
f(x);start;end;step
f(x);g(x);start;end;step
dtable:f(x);start;end;step
```

Tối đa 16 row. Range có thể tăng hoặc giảm nhưng dấu step phải đi về phía end. Derivative table chỉ hỗ trợ một function và dùng sai phân trung tâm.

### 8: VECTOR

Vector có 2 hoặc 3 thành phần và bốn vùng nhớ cố định `VctA`, `VctB`, `VctC`, `VctD`. Register chưa định nghĩa mở dạng hàng 1×3 trong một cặp ngoặc vuông lớn; điều khiển lưu/xóa giống MATRIX.

```text
norm(VctA)
scale(2;VctA)
dot(VctA;VctB)
cross(VctA;VctB)
angle(VctA;VctB)
```

Action selector và keypad dùng tên `f()`/register tương ứng. Hai operand của dot/angle phải cùng số chiều; cross chỉ nhận 3D. Angle dùng angle mode hiện tại. Vector inline và alias `name:...` cũ vẫn được engine nhận để tương thích.

### 9: INEQ

```text
left<right                  hoặc <=, >, >=; giải tuyến tính theo X
quad:left<right             giải bất đẳng thức bậc hai theo X
ineq2(a;b;c;relation)       grid hệ số bậc 2
ineq3(a;b;c;d;relation)     grid hệ số bậc 3
ineq4(a;b;c;d;e;relation)   grid hệ số bậc 4
```

UI có ba action DEGREE 2/3/4 và relation `<`, `>`, `<=`, `>=`. Dạng relation expression (`quad(...)`, `cubic(...)`, `quartic(...)`) nội suy hệ số số học; caller phải đưa đúng lớp hàm vì module không chứng minh symbolic bậc của input. Dạng `ineq2/3/4` nhận hệ số trực tiếp từ grid.

### 10: RATIO

```text
a:b                         rút gọn tỷ số
a:?=c:d                     tìm số hạng thứ hai
?:b=c:d                     tìm số hạng thứ nhất
a:b=?:d                     tìm số hạng thứ ba
a:b=c:?                     tìm số hạng cuối
```

Tỷ số thực được xấp xỉ bằng continued fraction tối đa 32 bước với tolerance tương đối 5×10^-9 và giới hạn signed 64-bit cho tử/mẫu. Đây là quy ước thực dụng, không phải rational-arithmetic chính xác tùy ý.

### 11: DIST

```text
n;p;k                       binomial PDF mặc định (compatibility order)
binompdf(k;n;p)             binomcdf(k;n;p)
poissonpdf:k;lambda         poisson:k;lambda
poissoncdf:k;lambda
normpdf(x;sigma;mu)
normcdf(lower;upper;sigma;mu)
norminv(area;sigma;mu)
normalpdf:x;mean;sigma      compatibility spelling/order
normalcdf:x;mean;sigma      normal:x;mean;sigma
normalinv:area;mean;sigma
geompdf:k;p                 geomcdf:k;p
hypergeom:N;K;n;k           hypergeometric:N;K;n;k
```

Canonical binomial call dùng thứ tự `k;n;p`; dạng colon cũ giữ `n;p;k`. Canonical normal CDF nhận cả lower/upper; spelling `normal*` ba đối số được giữ để tương thích. Poisson PDF/CDF dùng `k;lambda`; các tham số đếm phải là số nguyên không âm.

### 12: GRAPHING

Graph function list nhận tối đa 10 row:

```text
x^2;sin(x)                  hai hàng y= ngầm định
y=x^2
x=y^2
r=2sin(t)
param(cos(t);sin(t))         canonical
param:cos(t);sin(t)          compatibility
y<x^2                       y<=, y>, y>= tương tự
x>=y^2                      x<, x<=, x> tương tự
```

Dấu `;` giữa hai thành phần parametric thuộc cùng row; dấu `;` tiếp theo bắt đầu row mới. Formula trên mỗi vế bị giới hạn bởi `CALC_SYNTAX_TEXT` (63 byte + NUL).

UI contract của GRAPH:

- chọn mode GRAPHING mở expression input trước, không plot ngay;
- expression input có keypad 6×5 trên bốn trang, glyph 5×7 và `EXE` cố định ở ô dưới cùng bên phải;
- D-pad vượt mép lưới đổi trang, `A` tại `EXE` (hoặc `START`) plot source hiện tại;
- `B` trong viewport quay lại expression input và không làm mất source/cursor;
- buffer biểu thức GRAPH độc lập với buffer COMP.

## Coverage của host tests

Các file test hiện có, không kèm tuyên bố pass trong tài liệu tĩnh:

| Suite | Contract được kiểm tra |
|---|---|
| `tests/app_runtime_test.c` | lifecycle/render, menu return, editor isolation, keypad COMP/GRAPH 6×5, GRAPH EXE/B, table và streaming graph |
| `tests/app_workspace_test.c` | route và page riêng từng mode, toàn bộ slot keypad, form/grid, matrix chữ nhật, INEQ 2–4, STAT đủ 8 loại và BASE-N UI |
| `tests/core_test.c` | parser, implicit multiplication, decimal comma, relation, memories/context isolation, AST cache lặp/mutate, series/calculus và angle mode |
| `tests/decimal_natural_test.c` | decimal arithmetic/rounding, long division, harmonic accumulation, slot và edit Natural cursor |
| `tests/evaluator_catalog_test.c` | catalogue evaluator và parser graph-row |
| `tests/gfx_test.c` | glyph 5×7, clipping, fill rectangle, guard và stride padding |
| `tests/graph_discontinuity_test.c` | domain/pole/discontinuity khi stream graph |
| `tests/memory_budget_test.c` | budget cấu trúc AppState/graph và graph chunk |
| `tests/mode_command_contract_test.c` | cú pháp `name(...)`, alias colon lịch sử, ownership theo mode và parametric graph |
| `tests/modes_graph_test.c` | ví dụ đại diện cho 11 mode tính, typed graph row, streaming chunk và world clipping |
| `tests/stat_base_contract_test.c` | tám STAT model/frequency kernels; BASE radix/digit/arithmetic/overflow, bù hai, XNOR và Neg |

Kết quả thực tế phải lấy từ lần chạy `scripts/build.ps1 host-test` hoặc `scripts/build.ps1 verify` trên checkout đang xét.

## Giới hạn phải giữ rõ trong mọi mô tả

- Đây là prototype chức năng, không phải clone đầy đủ fx-9750GII/GIII, fx-570VN PLUS hay fx-CG100.
- Không có claim “453 function/command”; catalogue phía trên là contract hữu hạn của source.
- `DecimalNumber` chưa là numeric backend chung.
- Calculus, equation solving, regression và distribution đều là numerical algorithms hữu hạn độ chính xác.
- Graph discontinuity dùng sample state, detector tangent và midpoint bridge sâu 2 mức; không có symbolic singularity analysis.
- Inequality graph không phải solver miền hai biến tổng quát.
- Không có spreadsheet, eActivity, program editor, conics, dynamic graph, CAS hay Graph Solve đầy đủ.
- Host tests không thay emulator/hardware test; chưa có visual emulator verification trong baseline tài liệu này.
