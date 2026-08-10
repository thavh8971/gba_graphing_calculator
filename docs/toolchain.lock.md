# Toolchain contract và snapshot tham chiếu

File này khóa **contract của toolchain** và ghi snapshot môi trường dùng để phát triển. Nó không vendor compiler/libgba vào repository và không tự biến package cài toàn hệ thống thành dependency bất biến. Kết quả test, size và SHA-256 của ROM không được ghi cứng ở đây; chúng phải được sinh từ checkout hiện tại trong bước verify.

## Snapshot tham chiếu

Snapshot quan sát trên máy phát triển ngày 2026-08-08:

| Thành phần | Package/version quan sát |
|---|---|
| devkitARM meta package | `devkitARM-r68-1` |
| ARM GCC | `devkitarm-gcc-16.1.0-1` (`arm-none-eabi-gcc 16.1.0`) |
| ARM binutils | `devkitarm-binutils-2.46.0-1` (`objcopy 2.46.0.20260210`) |
| ARM newlib | `devkitarm-newlib-4.6.0.20260123-5` |
| ARM CRT files | `devkitarm-crtls-1.2.6-1` |
| libgba | `libgba-0.5.4-1` |
| gbafix/GBA tools | `gba-tools-1.2.0-2` |
| GNU Make | `make-4.4.1-3` (`GNU Make 4.4.1`) |
| Host GCC | `mingw-w64-ucrt-x86_64-gcc-16.1.0-6` (`gcc 16.1.0`) |
| PowerShell tham chiếu | Windows PowerShell `5.1.26100.3624` |

Đây là mốc tái lập ưu tiên, không phải minimum-version claim. Một version devkitPro khác có thể build được nhưng phải được coi là môi trường mới và cần chạy lại toàn bộ verify.

## Bố cục toolchain cần có

Với một devkitPro root `<DEVKITPRO>`, script yêu cầu các file:

```text
<DEVKITPRO>/devkitARM/bin/arm-none-eabi-gcc.exe
<DEVKITPRO>/libgba/include/gba.h
<DEVKITPRO>/libgba/lib/libgba.a
<DEVKITPRO>/tools/bin/gbafix.exe
```

Trên Windows, `scripts/build.ps1` tìm root theo thứ tự:

1. tham số `-DevkitPro`;
2. biến môi trường `DEVKITPRO` nếu là đường dẫn Windows, không bắt đầu bằng `/`;
3. `C:\devkitPro`;
4. `C:\msys64\opt\devkitpro`.

GNU Make được lấy từ `-Make`, rồi từ `PATH`, rồi `C:\msys64\usr\bin\make.exe`. Nếu có MSYS2, script dùng `bash.exe` làm `MAKESHELL` để chạy recipe POSIX và thêm Unix utilities vào `PATH`. Target host-only không bắt buộc devkitARM, nhưng vẫn cần GNU Make, host GCC, libm và shell/utilities tương thích recipe.

## Cách gọi chuẩn

Từ PowerShell tại root repository:

```powershell
# Chỉ host tests
.\scripts\build.ps1 host-test

# Chỉ ROM GBA
.\scripts\build.ps1 gba

# Rebuild host tests + ROM và đọc lại cartridge header
.\scripts\build.ps1 verify -Rebuild

# Xác minh header, rồi sinh size và SHA-256 của ROM hiện tại
.\scripts\build.ps1 rom-info

# Xóa thư mục build
.\scripts\build.ps1 clean
```

Override tường minh:

```powershell
.\scripts\build.ps1 verify -Rebuild `
  -DevkitPro C:\msys64\opt\devkitpro `
  -Make C:\msys64\usr\bin\make.exe
```

Target mặc định `all` của Makefile hiện tương đương `gba`, không chạy host tests. Dùng `verify` khi cần một gate tích hợp.

## Build contract

### GBA

| Thuộc tính | Giá trị |
|---|---|
| CPU/tune | `arm7tdmi` |
| ISA | Thumb + interwork cho C; fast framebuffer span-fill là ARM assembly |
| C dialect | `gnu99` |
| Optimization/debug | `-O2 -g` |
| Warnings | `-Wall -Wextra -Wpedantic` |
| Aliasing | `-fno-strict-aliasing` |
| Sectioning | `-ffunction-sections -fdata-sections`, linker `--gc-sections` |
| Specs | `gba.specs` |
| Libraries | `libgba`, `libm` |

Artifacts do Makefile quản lý:

```text
gba_graphing_calculator.gba
build/gba/gba_graphing_calculator.elf
build/gba/gba_graphing_calculator.gba
build/gba/gba_graphing_calculator.map
```

File `.gba` ở root là bản phát hành canonical và phải giống byte-for-byte với ROM dưới `build/gba`.

### Host

| Thuộc tính | Giá trị mặc định |
|---|---|
| Compiler | `gcc` hoặc `HOST_CC` override |
| C dialect | `c99` |
| Flags chung | cùng warning/optimization/section flags với core portable |
| Libraries | `-lm` |
| Binary | một executable cho mỗi `tests/*_test.c` hoặc `tests/test_*.c` |

Host build cố ý loại `source/platform/gba_main.c` để không phụ thuộc libgba.

## Cartridge metadata cố định

`gbafix` được gọi với:

| Field | Giá trị |
|---|---|
| Title (12 byte) | `GBAGRAPHCALC` |
| Game code (4 byte) | `AGCE` |
| Maker code (2 byte) | `01` |
| Version | `0` |

`verify-rom-header` đọc lại byte tại offset header của file `.gba`; nó không chỉ tin biến Make. Target này không xác minh gameplay, render hoặc boot trên thiết bị.

`verify-arm-fast` dùng `arm-none-eabi-readelf` đọc `st_value` của `gcalcArmFill16` trong ELF sau link. Gate yêu cầu symbol nằm ở vùng địa chỉ `03...` của IWRAM và có bit ISA chẵn (ARM, không phải Thumb). Việc source ghi `.arm`/`.iwram` chưa đủ nếu linker artifact không giữ đúng contract này.

## Dữ liệu sinh trong verify

Không copy SHA-256 của project cũ sang project này. Với mỗi bản cần bàn giao, lấy các trường sau từ chính lần chạy tích hợp:

| Trường bàn giao | Nguồn chuẩn |
|---|---|
| Host test pass/fail | output của `scripts/build.ps1 verify -Rebuild` |
| Cartridge-header pass/fail | output `ROM header verified.` của cùng lệnh |
| ARM/IWRAM fast-fill pass/fail | output `ARM/IWRAM framebuffer fast path verified.` của cùng lệnh |
| ELF/ROM size | output linker/`wc -c` tại lần verify/`rom-info` |
| ROM SHA-256 | output của `scripts/build.ps1 rom-info` |
| Toolchain thực dùng | version command/package database của môi trường chạy verify |
| Visual/emulator status | ghi riêng; không suy ra từ host tests hoặc ROM header |

Nếu một trường chưa được sinh, ghi “chưa verify” thay vì dùng số từ lần build, project hay toolchain khác.

## Mức tái lập thực tế

Makefile ổn định thứ tự source bằng `sort`, đặt metadata ROM cố định và không nhúng timestamp qua macro của project. Tuy vậy, bit-for-bit reproducibility vẫn phụ thuộc version GCC/binutils/newlib/libgba/gbafix và môi trường archive/link. Snapshot trên là baseline để điều tra khác biệt, không phải bằng chứng rằng mọi máy sẽ sinh cùng hash.
