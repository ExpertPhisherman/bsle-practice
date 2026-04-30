# !!! SHARED MAKEFILE: NOT INTENDED FOR USE OUTSIDE OF A PROJECT DIRECTORY !!!

# Compiler and flags
CC = cc
CFLAGS = -std=gnu11 -Wall -Werror -Wextra -Wformat=2 -Wstrict-prototypes -Wwrite-strings -Wvla -Wfloat-equal -pthread -D_POSIX_C_SOURCE=200809L
CPPFLAGS = -Iinclude $(patsubst %,-I%/include,$(LIB_DIRS))
AR = ar
ARFLAGS = rcs
RM = rm -f

# Folders
SRC = src
INCLUDE = include
BUILD = build
BIN = bin

.DEFAULT_GOAL := all

# Local files
SRCS = $(wildcard $(SRC)/*.c)
MAIN_SRC = $(SRC)/main.c
LIB_SRCS = $(filter-out $(MAIN_SRC),$(SRCS))

OBJS = $(patsubst $(SRC)/%.c,$(BUILD)/%.o,$(SRCS))
LIB_OBJS = $(patsubst $(SRC)/%.c,$(BUILD)/%.o,$(LIB_SRCS))
DEPS = $(OBJS:.o=.d)

# External static libraries derived from LIB_DIRS
EXT_LIBS = $(foreach d,$(LIB_DIRS),$(d)/bin/lib$(notdir $(d)).a)

# Outputs
LIB = $(BIN)/lib$(NAME).a
TARGET = $(BIN)/main

# Does this project have a main.c?
ifeq ($(wildcard $(MAIN_SRC)),$(MAIN_SRC))
ALL_TARGETS = $(LIB) $(TARGET)
$(TARGET): $(OBJS) $(EXT_LIBS)
	@mkdir -p $(BIN)
	$(CC) $(CFLAGS) $(OBJS) -Wl,--start-group $(EXT_LIBS) -Wl,--end-group -o $@
else
ALL_TARGETS = $(LIB)
endif

$(EXT_LIBS):
	$(MAKE) -C $(patsubst %/bin/,%,$(dir $@)) all

# Clang-Tidy settings
TIDY = clang-tidy

TIDY_CHECKS = -*, \
	hicpp-braces-around-statements, \
	hicpp-signed-bitwise, \
	cppcoreguidelines-pro-bounds-array-to-pointer-decay, \
	readability-braces-around-statements, \
	readability-identifier-naming, \
	readability-function-size, \
	readability-misleading-indentation, \
	readability-implicit-bool-conversion, \
	readability-avoid-const-params-in-decls

.PHONY: all debug clean tidy tidy-fix

# Default target
all: $(ALL_TARGETS)

# Static library for this project
$(LIB): $(LIB_OBJS)
	@mkdir -p $(BIN)
	$(AR) $(ARFLAGS) $@ $^

# Compile objects and generate dependency files automatically
$(BUILD)/%.o: $(SRC)/%.c
	@mkdir -p $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) -MMD -MP -c $< -o $@

# Pull in auto-generated dependency files
-include $(DEPS)

# Debug
debug: CFLAGS += -ggdb3 -Og -DDEBUG
debug: clean all

# Clang-Tidy checks
tidy:
	$(TIDY) -header-filter='.*' -checks='$(TIDY_CHECKS)' $(SRCS) -- $(CPPFLAGS) $(CFLAGS)

tidy-fix:
	$(TIDY) -header-filter='.*' -checks='$(TIDY_CHECKS)' $(SRCS) -fix -- $(CPPFLAGS) $(CFLAGS)

# Cleanup
clean:
	$(RM) -r $(BIN) $(BUILD)