# ── Toolchain ────────────────────────────────────────────────────────
CC      := gcc
CFLAGS  := -Wall -Wextra -O2 -std=c11
LDFLAGS :=

# ── Platform bits ────────────────────────────────────────────────────
ifeq ($(OS),Windows_NT)
  EXE := .exe
  RM  := del /Q
  RUN :=
else
  EXE :=
  RM  := rm -f
  RUN := ./
endif

PREFIX  ?= /usr/local
BINDIR  ?= $(PREFIX)/bin

TARGET  := autogit$(EXE)
SRC     := autogit.c
OBJ     := autogit.o

.PHONY: all run clean install uninstall test

# ── Build ────────────────────────────────────────────────────────────
all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

# ── Run ──────────────────────────────────────────────────────────────
run: $(TARGET)
	$(RUN)$(TARGET)

test: $(TARGET)
	$(RUN)$(TARGET) -h

# ── Housekeeping ─────────────────────────────────────────────────────
clean:
	-$(RM) $(TARGET) $(OBJ)

# ── Install ──────────────────────────────────────────────────────────
ifeq ($(OS),Windows_NT)
install:
	@echo Copy $(TARGET) into any folder on PATH, e.g.:
	@echo   copy $(TARGET) "%USERPROFILE%\\bin\\"
uninstall:
	@echo Windows: just delete $(TARGET) from wherever you copied it.
else
install: $(TARGET)
	install -d $(DESTDIR)$(BINDIR)
	install -m 755 $(TARGET) $(DESTDIR)$(BINDIR)/$(TARGET)
uninstall:
	$(RM) $(DESTDIR)$(BINDIR)/$(TARGET)
endif