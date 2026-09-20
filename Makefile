CC = gcc

CPP_FLAGS = -Isrc
WARN_FLAGS = -Wall -Wextra
ARCH_FLAGS ?= -march=native
CFLAGS ?= $(WARN_FLAGS) -O2 $(ARCH_FLAGS)
LDLIBS ?=

BUILD_DIR ?= build
TARGET ?= $(BUILD_DIR)/r2p

SRC := $(shell find src -name '*.c')
OBJ := $(patsubst src/%.c,$(BUILD_DIR)/%.o,$(SRC))
DEP := $(OBJ:.o=.d)

.PHONY: all clean test test_lexer test_parser test_sema test_ir

all: $(TARGET)

$(TARGET): $(OBJ)
	@mkdir -p $(dir $@)
	$(CC) $(OBJ) $(LDLIBS) -o $@

$(BUILD_DIR)/%.o: src/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CPP_FLAGS) $(CFLAGS) -MMD -MP -c $< -o $@

test: test_lexer test_parser test_sema test_ir

test_lexer: $(TARGET)
	@tests/run_suite.sh $(TARGET) --dump-tokens tests/lexer

test_parser: $(TARGET)
	@tests/run_suite.sh $(TARGET) --dump-ast tests/parser

test_sema: $(TARGET)
	@tests/run_suite.sh $(TARGET) --dump-symbols tests/sema

test_ir: $(TARGET)
	@tests/run_suite.sh $(TARGET) --dump-ir tests/ir

-include $(DEP)

clean:
	rm -rf build/
