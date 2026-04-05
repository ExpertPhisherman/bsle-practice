# Compiler and flags
CFLAGS = -std=c11 -Wall -Werror -Wextra -Wformat=2 -Wpedantic -Wstrict-prototypes -Wwrite-strings -Wvla -Wfloat-equal -pthread -D_POSIX_C_SOURCE=200809L

# Folders
SRC = src
INCLUDE = include
BUILD = build
BIN = bin

# Files
SRCS = $(wildcard $(SRC)/*.c)
OBJS = $(patsubst $(SRC)/%.c, $(BUILD)/%.o, $(SRCS))
HDRS = $(wildcard $(INCLUDE)/*.h)

TARGET = $(BIN)/main

# Clang-Tidy settings
TIDY = clang-tidy
TIDY_CHECKS = "-*,readability-*,hicpp-braces-around-statements, \
               hicpp-uppercase-literal-suffix,hicpp-signed-bitwise, \
               cppcoreguidelines-pro-bounds-array-to-pointer-decay, \
               readability-braces-around-statements, \
               readability-identifier-naming, \
               readability-function-size, \
               readability-misleading-indentation, \
               readability-implicit-bool-conversion, \
               readability-avoid-const-params-in-decls"

# Default target
all: $(TARGET)

$(TARGET): $(OBJS)
	@mkdir -p $(BIN)
	$(CC) $(CFLAGS) $^ -o $@

$(BUILD)/%.o: $(SRC)/%.c $(HDRS)
	@mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -I$(INCLUDE) -c $< -o $@

# Debug build
.PHONY: debug
debug: CFLAGS += -ggdb3 -DDEBUG
debug: all
# Clang-Tidy checks
tidy:
	$(TIDY) $(SRCS) -- -I$(INCLUDE) -std=c11 -D_POSIX_C_SOURCE=200809L
tidy-fix:
	$(TIDY) $(SRCS) -fix -- -I$(INCLUDE) -std=c11 -D_POSIX_C_SOURCE=200809L
# Cleanup
clean:
	$(RM) -r $(BIN) $(BUILD)

