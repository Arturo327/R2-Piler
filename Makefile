CC = gcc

INC_FLAGS = -Isrc
WARN_FLAGS = -Wall -Wextra
ARCH_FLAGS ?= -march=native
CFLAGS ?= $(INC_FLAGS) $(WARN_FLAGS) -O2 $(ARCH_FLAGS)
LDLIBS ?=

BUILD_DIR ?= build
TARGET ?= $(BUILD_DIR)/r2p

SRC := $(shell find src -name '*.c')
OBJ := $(patsubst src/%.c,$(BUILD_DIR)/%.o,$(SRC))
DEP := $(OBJ:.o=.d)
LEXER_SRCS := $(sort $(wildcard tests/lexer/*_src.r2))
PARSER_SRCS := $(sort $(wildcard tests/parser/*_src.r2))

.PHONY: all clean test test_lexer test_parser

all: $(TARGET)

$(TARGET): $(OBJ)
	@mkdir -p $(dir $@)
	$(CC) $(OBJ) $(LDLIBS) -o $@

$(BUILD_DIR)/%.o: src/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

test: test_lexer test_parser

test_lexer: $(TARGET)
	@if [ -z "$(LEXER_SRCS)" ]; then echo "No lexer tests found"; exit 1; fi; \
	passed=0; \
	failed=0; \
	for src in $(LEXER_SRCS); do \
		base="$${src%_src.r2}"; \
		expected_out="$${base}_result.txt"; \
		expected_err="$${base}_stderr.txt"; \
		expected_status="$${base}_status.txt"; \
		tmp_out="$$(mktemp)"; \
		tmp_err="$$(mktemp)"; \
		"$(TARGET)" --dump-tokens "$$src" >"$$tmp_out" 2>"$$tmp_err"; \
		actual_status=$$?; \
		want_status=0; \
		if [ -f "$$expected_status" ]; then want_status="$$(cat "$$expected_status")"; fi; \
		ok=1; \
		if [ ! -f "$$expected_out" ]; then echo "FAIL $$src: missing $$expected_out"; ok=0; \
		elif [ "$$actual_status" -ne "$$want_status" ]; then echo "FAIL $$src: status $$actual_status, want $$want_status"; ok=0; \
		elif ! diff -u "$$expected_out" "$$tmp_out"; then echo "FAIL $$src: stdout differs"; ok=0; \
		elif [ -f "$$expected_err" ]; then if ! diff -u "$$expected_err" "$$tmp_err"; then echo "FAIL $$src: stderr differs"; ok=0; fi; \
		elif [ -s "$$tmp_err" ]; then echo "FAIL $$src: unexpected stderr:"; cat "$$tmp_err"; ok=0; \
		fi; \
		if [ "$$ok" -eq 1 ]; then echo "PASS $$src"; passed=$$((passed+1)); else failed=$$((failed+1)); fi; \
		rm -f "$$tmp_out" "$$tmp_err"; \
	done; \
	echo "$$passed passed, $$failed failed"; \
	test "$$failed" -eq 0

test_parser: $(TARGET)
	@if [ -z "$(PARSER_SRCS)" ]; then echo "No parser tests found"; exit 1; fi; \
	passed=0; \
	failed=0; \
	for src in $(PARSER_SRCS); do \
		base="$${src%_src.r2}"; \
		expected_out="$${base}_result.txt"; \
		expected_err="$${base}_stderr.txt"; \
		expected_status="$${base}_status.txt"; \
		tmp_out="$$(mktemp)"; \
		tmp_err="$$(mktemp)"; \
		"$(TARGET)" --dump-ast "$$src" >"$$tmp_out" 2>"$$tmp_err"; \
		actual_status=$$?; \
		want_status=0; \
		if [ -f "$$expected_status" ]; then want_status="$$(cat "$$expected_status")"; fi; \
		ok=1; \
		if [ ! -f "$$expected_out" ]; then echo "FAIL $$src: missing $$expected_out"; ok=0; \
		elif [ "$$actual_status" -ne "$$want_status" ]; then echo "FAIL $$src: status $$actual_status, want $$want_status"; ok=0; \
		elif ! diff -u "$$expected_out" "$$tmp_out"; then echo "FAIL $$src: stdout differs"; ok=0; \
		elif [ -f "$$expected_err" ]; then if ! diff -u "$$expected_err" "$$tmp_err"; then echo "FAIL $$src: stderr differs"; ok=0; fi; \
		elif [ -s "$$tmp_err" ]; then echo "FAIL $$src: unexpected stderr:"; cat "$$tmp_err"; ok=0; \
		fi; \
		if [ "$$ok" -eq 1 ]; then echo "PASS $$src"; passed=$$((passed+1)); else failed=$$((failed+1)); fi; \
		rm -f "$$tmp_out" "$$tmp_err"; \
	done; \
	echo "$$passed passed, $$failed failed"; \
	test "$$failed" -eq 0

-include $(DEP)

clean:
	rm -rf build/
