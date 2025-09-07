CC := clang
BUILD ?= debug
CFLAGS_COMMON := -Wall -Wextra -Wno-sign-compare
LDFLAGS :=

ifeq ($(BUILD),debug)
    CFLAGS := $(CFLAGS_COMMON) -g -O0
    BINDIR := build/debug
else ifeq ($(BUILD),release)
    CFLAGS := $(CFLAGS_COMMON) -O3 -DNDEBUG
    BINDIR := build/release
endif

OBJDIR := $(BINDIR)/obj
SRC := $(wildcard src/*.c)
OBJ := $(patsubst src/%.c, $(OBJDIR)/%.o, $(SRC))
BIN := $(BINDIR)/ascc

.PHONY: all clean debug release count gdb

all: $(BIN)

debug:
	$(MAKE) BUILD=debug

release:
	$(MAKE) BUILD=release

$(BIN): $(OBJ)
	@mkdir -p $(dir $@)
	@$(CC) $(OBJ) -o $@ $(LDFLAGS)

$(OBJDIR)/%.o: src/%.c
	@mkdir -p $(dir $@)
	@echo Building $<
	@$(CC) $(CFLAGS) -c $< -o $@

clean:
	@rm -rf build

count:
	@find ./src -type f \( -name "*.c" -o -name "*.h" \) -exec wc -l {} +

gdb: debug
	@gdb $(BINDIR)/ascc
