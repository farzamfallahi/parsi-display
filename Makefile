# Host-side build for ps_shaper (GNU make).
.RECIPEPREFIX = >

CC      ?= cc
CFLAGS  := -std=c99 -Wall -Wextra -Wpedantic -Werror -O2
CPPFLAGS := -Ishaper/include

BUILD   := build
LIB_SRC := shaper/src/ps_shaper.c
HEADER  := shaper/include/ps_shaper.h

TEST    := $(BUILD)/test_shaper
DEMO    := $(BUILD)/ps_demo
SHLIB   := $(BUILD)/libpsshaper.so

.PHONY: all test clean

all: $(TEST) $(DEMO) $(SHLIB)

$(BUILD):
> mkdir -p $@

$(TEST): tests/test_shaper.c $(LIB_SRC) $(HEADER) | $(BUILD)
> $(CC) $(CPPFLAGS) $(CFLAGS) tests/test_shaper.c $(LIB_SRC) -o $@

$(DEMO): tools/ps_demo.c $(LIB_SRC) $(HEADER) | $(BUILD)
> $(CC) $(CPPFLAGS) $(CFLAGS) tools/ps_demo.c $(LIB_SRC) -o $@

$(SHLIB): $(LIB_SRC) $(HEADER) | $(BUILD)
> $(CC) $(CPPFLAGS) $(CFLAGS) -fPIC -shared $(LIB_SRC) -o $@

test: $(TEST)
> ./$(TEST)

clean:
> rm -rf $(BUILD)
