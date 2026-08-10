# Reproducible host and Game Boy Advance builds for gba_graphing_calculator.
#
# Required for GBA targets:
#   DEVKITPRO=/path/to/devkitpro
#   DEVKITARM=$(DEVKITPRO)/devkitARM   (optional override)
#   LIBGBA=$(DEVKITPRO)/libgba         (optional override)
#
# On Windows, scripts/build.ps1 discovers the usual devkitPro installation and
# passes paths in the form expected by GNU Make and devkitARM.

.SUFFIXES:

PROJECT := gba_graphing_calculator

SOURCE_DIR := source
INCLUDE_DIR := include
TEST_DIR := tests
GBA_BUILD := build/gba
HOST_BUILD := build/host

# Keep cartridge identification deterministic.  GBA limits are 12, 4 and 2
# bytes respectively; these values deliberately use exactly those limits.
override ROM_TITLE := GBAGRAPHCALC
override ROM_GAME_CODE := AGCE
override ROM_MAKER_CODE := 01
override ROM_VERSION := 0

# The graph parser/evaluator peaks at roughly 17.1 KiB of user stack.
GBA_MIN_STACK_BYTES := 18432

GBA_ELF := $(GBA_BUILD)/$(PROJECT).elf
GBA_ROM := $(GBA_BUILD)/$(PROJECT).gba
GBA_MAP := $(GBA_BUILD)/$(PROJECT).map
RELEASE_ROM := $(PROJECT).gba

DEVKITARM ?= $(DEVKITPRO)/devkitARM
LIBGBA ?= $(DEVKITPRO)/libgba

ARM_CC ?= $(DEVKITARM)/bin/arm-none-eabi-gcc
ARM_OBJCOPY ?= $(DEVKITARM)/bin/arm-none-eabi-objcopy
ARM_OBJDUMP ?= $(DEVKITARM)/bin/arm-none-eabi-objdump
ARM_SIZE ?= $(DEVKITARM)/bin/arm-none-eabi-size
ARM_NM ?= $(DEVKITARM)/bin/arm-none-eabi-nm
ARM_READELF ?= $(DEVKITARM)/bin/arm-none-eabi-readelf
GBAFIX ?= $(DEVKITPRO)/tools/bin/gbafix
HOST_CC ?= gcc
SHA256 ?= sha256sum

COMMON_SOURCES := $(sort \
	$(wildcard $(SOURCE_DIR)/*.c) \
	$(wildcard $(SOURCE_DIR)/core/*.c) \
	$(wildcard $(SOURCE_DIR)/app/*.c) \
	$(wildcard $(SOURCE_DIR)/graph/*.c) \
	$(wildcard $(SOURCE_DIR)/modes/*.c) \
	$(wildcard $(SOURCE_DIR)/ui/*.c))
GBA_SOURCES := $(COMMON_SOURCES) $(SOURCE_DIR)/platform/gba_main.c
GBA_ASM_SOURCES := $(sort $(wildcard $(SOURCE_DIR)/platform/*.S))

GBA_OBJECTS := $(patsubst %.c,$(GBA_BUILD)/%.o,$(GBA_SOURCES)) \
	$(patsubst %.S,$(GBA_BUILD)/%.o,$(GBA_ASM_SOURCES))
GBA_DEPS := $(GBA_OBJECTS:.o=.d)

HOST_COMMON_OBJECTS := $(patsubst %.c,$(HOST_BUILD)/%.o,$(COMMON_SOURCES))
TEST_SOURCES := $(sort $(wildcard $(TEST_DIR)/*_test.c) \
	$(wildcard $(TEST_DIR)/test_*.c))

ifeq ($(OS),Windows_NT)
HOST_EXEEXT := .exe
else
HOST_EXEEXT :=
endif

HOST_TEST_OBJECTS := $(patsubst %.c,$(HOST_BUILD)/%.o,$(TEST_SOURCES))
HOST_TEST_BINS := $(patsubst $(TEST_DIR)/%.c,$(HOST_BUILD)/%$(HOST_EXEEXT),$(TEST_SOURCES))
HOST_DEPS := $(HOST_COMMON_OBJECTS:.o=.d) $(HOST_TEST_OBJECTS:.o=.d)

CPPFLAGS := -I$(INCLUDE_DIR)
WARNINGS := -Wall -Wextra -Wpedantic
COMMON_CFLAGS := -O2 -g $(WARNINGS) -fno-strict-aliasing \
	-ffunction-sections -fdata-sections
GBA_ARCH := -mcpu=arm7tdmi -mtune=arm7tdmi -mthumb -mthumb-interwork
GBA_CFLAGS := $(COMMON_CFLAGS) -std=gnu99 $(GBA_ARCH)
GBA_LDFLAGS := $(GBA_ARCH) -specs=gba.specs \
	-Wl,-Map,$(GBA_MAP),--gc-sections
HOST_CFLAGS ?= $(COMMON_CFLAGS) -std=c99
HOST_LDFLAGS ?= -Wl,--gc-sections
HOST_LIBS ?= -lm

.PHONY: all gba host-test verify verify-rom-header verify-gba-memory \
	verify-arm-fast verify-ewram-caches verify-iwram-stack rom-info clean \
	check-gba-toolchain check-host-tests check-source-boundaries

all: gba

gba: $(RELEASE_ROM)

check-gba-toolchain:
	@test -n "$(DEVKITPRO)" || { echo "DEVKITPRO is not set." >&2; exit 1; }
	@test -x "$(ARM_CC)" || { echo "Missing devkitARM compiler: $(ARM_CC)" >&2; exit 1; }
	@test -x "$(ARM_OBJCOPY)" || { echo "Missing objcopy: $(ARM_OBJCOPY)" >&2; exit 1; }
	@test -x "$(ARM_OBJDUMP)" || { echo "Missing objdump: $(ARM_OBJDUMP)" >&2; exit 1; }
	@test -x "$(ARM_NM)" || { echo "Missing nm: $(ARM_NM)" >&2; exit 1; }
	@test -x "$(ARM_READELF)" || { echo "Missing readelf: $(ARM_READELF)" >&2; exit 1; }
	@test -f "$(LIBGBA)/include/gba.h" || { echo "Missing libgba headers under $(LIBGBA)" >&2; exit 1; }
	@test -f "$(LIBGBA)/lib/libgba.a" || { echo "Missing libgba archive under $(LIBGBA)" >&2; exit 1; }
	@test -x "$(GBAFIX)" || { echo "Missing gbafix: $(GBAFIX)" >&2; exit 1; }

$(GBA_BUILD)/%.o: %.c | check-gba-toolchain
	@mkdir -p "$(dir $@)"
	$(ARM_CC) $(CPPFLAGS) -DGCALC_GBA_ARM_FAST=1 \
		-isystem $(LIBGBA)/include $(GBA_CFLAGS) \
		-MMD -MP -MF "$(@:.o=.d)" -c "$<" -o "$@"

$(GBA_BUILD)/%.o: %.S | check-gba-toolchain
	@mkdir -p "$(dir $@)"
	$(ARM_CC) $(CPPFLAGS) -DGCALC_GBA_ARM_FAST=1 \
		-isystem $(LIBGBA)/include $(GBA_ARCH) \
		-x assembler-with-cpp -c "$<" -o "$@"

$(GBA_ELF): $(GBA_OBJECTS) | check-gba-toolchain
	@mkdir -p "$(dir $@)"
	$(ARM_CC) $(GBA_LDFLAGS) $(GBA_OBJECTS) -L$(LIBGBA)/lib \
		-lgba -lm -o "$@"
	$(ARM_SIZE) "$@"

$(GBA_ROM): $(GBA_ELF) | check-gba-toolchain
	$(ARM_OBJCOPY) -O binary "$<" "$@"
	$(GBAFIX) "$@" -t$(ROM_TITLE) -c$(ROM_GAME_CODE) \
		-m$(ROM_MAKER_CODE) -r$(ROM_VERSION)
	@echo "Built $@"

# Keep a canonical launchable ROM at the project root. This avoids silently
# opening a stale ROM from another checkout while retaining build/gba for ELF,
# map and intermediate artifacts.
$(RELEASE_ROM): $(GBA_ROM)
	cp "$<" "$@"
	@echo "Released $@"

$(HOST_BUILD)/%.o: %.c
	@mkdir -p "$(dir $@)"
	$(HOST_CC) $(CPPFLAGS) $(HOST_CFLAGS) -MMD -MP \
		-MF "$(@:.o=.d)" -c "$<" -o "$@"

$(HOST_BUILD)/%$(HOST_EXEEXT): $(HOST_BUILD)/$(TEST_DIR)/%.o $(HOST_COMMON_OBJECTS)
	@mkdir -p "$(dir $@)"
	$(HOST_CC) $(HOST_LDFLAGS) $^ $(HOST_LIBS) -o "$@"

check-host-tests:
	@test -n "$(strip $(HOST_TEST_BINS))" || { echo "No host tests found in $(TEST_DIR)." >&2; exit 1; }

check-source-boundaries:
	@if grep -R -n -E '#[[:space:]]*include[[:space:]]*"(\.\./|[A-Za-z]:[/\\])' \
		$(SOURCE_DIR) $(INCLUDE_DIR); then \
		echo "Source contains an include outside the project boundary." >&2; exit 1; \
	fi
	@if grep -R -n -E 'snes_graphing_calculator|C:[/\\]Users' \
		$(SOURCE_DIR) $(INCLUDE_DIR); then \
		echo "Source contains a machine- or sibling-project dependency." >&2; exit 1; \
	fi
	@echo "Source boundary check passed."

host-test: check-host-tests $(HOST_TEST_BINS)
	@set -e; for test_binary in $(HOST_TEST_BINS); do \
		echo "Running $$test_binary"; \
		"$$test_binary"; \
	done

# Verify the bytes gbafix wrote, not merely the Make variables used to invoke it.
verify-rom-header: $(RELEASE_ROM)
	@cmp -s "$(GBA_ROM)" "$(RELEASE_ROM)"
	@test "$$(dd if="$(RELEASE_ROM)" bs=1 skip=160 count=12 2>/dev/null | tr -d '\000')" = "$(ROM_TITLE)"
	@test "$$(dd if="$(RELEASE_ROM)" bs=1 skip=172 count=4 2>/dev/null)" = "$(ROM_GAME_CODE)"
	@test "$$(dd if="$(RELEASE_ROM)" bs=1 skip=176 count=2 2>/dev/null)" = "$(ROM_MAKER_CODE)"
	@test "$$(od -An -tu1 -j188 -N1 "$(RELEASE_ROM)" | tr -d ' ')" = "$(ROM_VERSION)"
	@echo "ROM header verified."

# The framebuffer fast path must be in the .iwram output section, fit wholly in
# physical IWRAM (0x03000000..0x03007fff), and retain an even ELF function
# address. ARM EABI uses bit zero on function symbols to mark Thumb state.
verify-arm-fast: $(GBA_ELF)
	@record="$$("$(ARM_OBJDUMP)" -t "$(GBA_ELF)" | \
		awk '$$6 == "gcalcArmFill16" { print $$1, $$3, $$4, $$5; exit }')"; \
		test -n "$$record" || { echo "Missing gcalcArmFill16." >&2; exit 1; }; \
		set -- $$record; address="$$1"; kind="$$2"; section="$$3"; size="$$4"; \
		test "$$kind" = F && test "$$section" = .iwram || { \
			echo "gcalcArmFill16 is not a function in .iwram: $$record" >&2; exit 1; }; \
		case "$$address$$size" in ''|*[!0-9A-Fa-f]*) \
			echo "Invalid gcalcArmFill16 address/size: $$record" >&2; exit 1 ;; esac; \
		start=$$((0x$$address)); bytes=$$((0x$$size)); end=$$((start + bytes)); \
		test "$$bytes" -gt 0 && test "$$start" -ge $$((0x03000000)) && \
			test "$$end" -le $$((0x03008000)) || { \
			echo "gcalcArmFill16 exceeds IWRAM: $$record" >&2; exit 1; }; \
		elf_address="$$("$(ARM_READELF)" -sW "$(GBA_ELF)" | \
			awk '$$8 == "gcalcArmFill16" && $$7 != "UND" { print $$2; exit }')"; \
		case "$$elf_address" in ''|*[!0-9A-Fa-f]*) \
			echo "Invalid gcalcArmFill16 ELF address: $$elf_address" >&2; exit 1 ;; esac; \
		test $$((0x$$elf_address & 1)) -eq 0 || { \
			echo "gcalcArmFill16 is not ARM state: $$elf_address" >&2; exit 1; }
	@echo "ARM/IWRAM framebuffer fast path verified."

# These parser caches are intentionally uninitialised EWRAM objects. Requiring
# .sbss also ensures the GBA CRT zeroes them through its .sbss bounds.
verify-ewram-caches: $(GBA_ELF)
	@set -e; for symbol in parseCache oversizeAst; do \
		record="$$("$(ARM_OBJDUMP)" -t "$(GBA_ELF)" | \
			awk -v name="$$symbol" '$$6 == name { print $$1, $$3, $$4, $$5; exit }')"; \
		test -n "$$record" || { echo "Missing $$symbol." >&2; exit 1; }; \
		set -- $$record; address="$$1"; kind="$$2"; section="$$3"; size="$$4"; \
		test "$$kind" = O && test "$$section" = .sbss || { \
			echo "$$symbol is not an uninitialised .sbss object: $$record" >&2; exit 1; }; \
		case "$$address$$size" in ''|*[!0-9A-Fa-f]*) \
			echo "Invalid $$symbol address/size: $$record" >&2; exit 1 ;; esac; \
		start=$$((0x$$address)); bytes=$$((0x$$size)); end=$$((start + bytes)); \
		test "$$bytes" -gt 0 && test "$$start" -ge $$((0x02000000)) && \
			test "$$end" -le $$((0x02040000)) || { \
			echo "$$symbol exceeds EWRAM: $$record" >&2; exit 1; }; \
	done
	@echo "EWRAM parser caches verified."

# Guard the measured graph parse/evaluate stack peak against later IWRAM data
# growth. Both linker-script symbols are absolute addresses in the final ELF.
verify-iwram-stack: $(GBA_ELF)
	@data_end="$$("$(ARM_OBJDUMP)" -t "$(GBA_ELF)" | \
		awk '$$3 == "*ABS*" && $$5 == "__data_end__" { print $$1; exit }')"; \
		sp_usr="$$("$(ARM_OBJDUMP)" -t "$(GBA_ELF)" | \
		awk '$$3 == "*ABS*" && $$5 == "__sp_usr" { print $$1; exit }')"; \
		case "$$data_end$$sp_usr" in ''|*[!0-9A-Fa-f]*) \
			echo "Missing or invalid IWRAM bounds: data=$$data_end stack=$$sp_usr" >&2; exit 1 ;; esac; \
		available=$$((0x$$sp_usr - 0x$$data_end)); \
		test "$$available" -ge "$(GBA_MIN_STACK_BYTES)" || { \
			echo "IWRAM user-stack margin is $$available bytes; need $(GBA_MIN_STACK_BYTES)." >&2; \
			exit 1; }; \
		echo "IWRAM user-stack margin verified ($$available bytes)."

verify-gba-memory: verify-arm-fast verify-ewram-caches verify-iwram-stack
	@echo "GBA memory layout verified."

verify: check-source-boundaries host-test verify-rom-header verify-gba-memory
	@echo "Host tests and GBA cartridge checks passed."

rom-info: verify-rom-header
	@echo "ROM:       $(RELEASE_ROM)"
	@echo "Title:     $(ROM_TITLE)"
	@echo "Game code: $(ROM_GAME_CODE)"
	@echo "Maker:     $(ROM_MAKER_CODE)"
	@echo "Version:   $(ROM_VERSION)"
	@wc -c "$(RELEASE_ROM)"
	@$(SHA256) "$(RELEASE_ROM)"

clean:
	rm -rf build
	rm -f "$(RELEASE_ROM)"

-include $(GBA_DEPS) $(HOST_DEPS)
