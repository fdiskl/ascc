CC := gcc
CFLAGS := -Wall -Wextra

SRC := $(wildcard src/*.c)
OBJ := $(patsubst src/%.c, build/%.o, $(SRC))
BIN := build/bin/ascc

.PHONY: all clean

all: $(BIN)

$(BIN): $(OBJ)
    @mkdir -p build/bin
    $(CC) $(OBJ) -o $@

build/%.o: src/%.c
	@mkdir -p build
	$(CC) $(CFLAGS) -c $< -o $@

clean:
    rm -rf build bin
