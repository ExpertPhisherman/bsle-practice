# Project name (ONLY thing that changes per project)
NAME = tpool

# External library project directories (ONLY other thing that changes per project)
LIB_DIRS =

# Compiler and flags
CC = cc
CFLAGS = -std=c11 -Wall -Werror -Wextra -Wformat=2 -Wpedantic -Wstrict-prototypes -Wwrite-strings -Wvla -Wfloat-equal -pthread -D_POSIX_C_SOURCE=200809L
CPPFLAGS = -Iinclude $(patsubst %,-I%/include,$(LIB_DIRS))
AR = ar
ARFLAGS = rcs
RM = rm -f

# Folders
SRC = src
INCLUDE = include
BUILD = build
BIN = bin

# Outputs
LIB = $(BIN)/lib$(NAME).a
TARGET = $(BIN)/$(NAME)

# Local files
SRCS = $(wildcard $(SRC)/*.c)
OBJS = $(patsubst $(SRC)/%.c,$(BUILD)/%.o,$(SRCS))
DEPS = $(OBJS:.o=.d)

# External static libraries derived from LIB_DIRS
EXT_LIBS = $(foreach d,$(LIB_DIRS),$(d)/bin/lib$(notdir $(d)).a)

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

.PHONY: all debug clean tidy tidy-fix FORCE

# Default target
all: $(LIB) $(TARGET)

# Always ask subprojects to rebuild themselves when their archive is checked.
# For ../tpool/bin/libtpool.a this recurses into ../tpool.
FORCE:

$(EXT_LIBS): FORCE
	$(MAKE) -C $(patsubst %/bin/,%,$(dir $@))

# Static library for this project
$(LIB): $(OBJS)
	@mkdir -p $(BIN)
	$(AR) $(ARFLAGS) $@ $^

# Executable for this project
$(TARGET): $(OBJS) $(EXT_LIBS)
	@mkdir -p $(BIN)
	$(CC) $(CFLAGS) $^ -o $@

# Compile objects and generate dependency files automatically
$(BUILD)/%.o: $(SRC)/%.c
	@mkdir -p $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) -MMD -MP -c $< -o $@

# Pull in auto-generated dependency files
-include $(DEPS)

# Debug
debug: CFLAGS += -ggdb3 -DDEBUG
debug: all

# Clang-Tidy checks
tidy:
	$(TIDY) $(SRCS) -- $(CPPFLAGS) -std=c11 -D_POSIX_C_SOURCE=200809L

tidy-fix:
	$(TIDY) $(SRCS) -fix -- $(CPPFLAGS) -std=c11 -D_POSIX_C_SOURCE=200809L

# Cleanup
clean:
	$(RM) -r $(BIN) $(BUILD)
