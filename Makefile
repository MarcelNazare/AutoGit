# ── Toolchain (always gcc) ───────────────────────────────────────────
CC      := gcc
CFLAGS  := -Wall -Wextra -O2 -std=c11
LDFLAGS :=

# ── Platform ─────────────────────────────────────────────────────────
ifeq ($(OS),Windows_NT)
  SHELL       := cmd.exe
  .SHELLFLAGS := /c
  EXE         := .exe
  WIN         := 1
else
  EXE         :=
endif

# ── Paths ────────────────────────────────────────────────────────────
BUILD  := build
TARGET := $(BUILD)/autogit$(EXE)
SRC    := autogit.c

# ── Per-platform command fragments ───────────────────────────────────
ifeq ($(WIN),1)
  winpath = $(subst /,\,$1)
  MKDIR_B = if not exist "$(call winpath,$(BUILD))" mkdir "$(call winpath,$(BUILD))"
  CLEAN_B = if exist "$(call winpath,$(BUILD))" rmdir /S /Q "$(call winpath,$(BUILD))"
  RUN_BIN = .\$(call winpath,$(TARGET))
  CHECK_GCC = where gcc >NUL 2>&1 || (echo ERROR: gcc not found in PATH. Install MinGW-w64. & exit /B 1)
else
  MKDIR_B = mkdir -p "$(BUILD)"
  CLEAN_B = rm -rf "$(BUILD)"
  RUN_BIN = ./$(TARGET)
  CHECK_GCC = command -v gcc >/dev/null 2>&1 || { echo "ERROR: gcc not found in PATH."; exit 1; }
endif

PREFIX ?= /usr/local
BINDIR ?= $(PREFIX)/bin

.DEFAULT_GOAL := all
.PHONY: all check build run test clean install uninstall

# ── Build ────────────────────────────────────────────────────────────
all: check $(TARGET)

check:
	@$(CHECK_GCC)

build: all

# Single-shot compile+link — same as `gcc ... -o build/autogit.exe autogit.c`
$(TARGET): $(SRC)
	@$(MKDIR_B)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $<

# ── Run ──────────────────────────────────────────────────────────────
run: $(TARGET)
	$(RUN_BIN) $(ARGS)

test: $(TARGET)
	$(RUN_BIN) -h

# ── Clean ────────────────────────────────────────────────────────────
clean:
	@$(CLEAN_B)

# ── Install ──────────────────────────────────────────────────────────
ifeq ($(WIN),1)
install: $(TARGET)
	@echo Copy $(call winpath,$(TARGET)) into a folder on PATH, e.g.:
	@echo   copy "$(call winpath,$(TARGET))" "%USERPROFILE%\\bin\\autogit.exe"
uninstall:
	@echo Windows: just delete autogit.exe from wherever you copied it.
else
install: $(TARGET)
	install -d $(DESTDIR)$(BINDIR)
	install -m 755 $(TARGET) $(DESTDIR)$(BINDIR)/autogit
uninstall:
	rm -f $(DESTDIR)$(BINDIR)/autogit
endif