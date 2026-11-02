CC = gcc
PYTHON ?= python3
CPPFLAGS += -Iinclude -D_POSIX_C_SOURCE=200809L -D_XOPEN_SOURCE=700 -D_FILE_OFFSET_BITS=64
CFLAGS = -std=c11 -Wall -Wextra -Wpedantic -g
TARGET = file_manager
SRC_DIR = src
BUILD_DIR = build

SOURCES := $(wildcard $(SRC_DIR)/*.c)
OBJECTS := $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(SOURCES))
DEPS := $(OBJECTS:.o=.d)
ATTR_TEST = $(BUILD_DIR)/test_attributes

.PHONY: all clean test

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) $(LDFLAGS) $(OBJECTS) $(LDLIBS) -o $@

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -MMD -MP -c $< -o $@

$(BUILD_DIR):
	mkdir -p $@

clean:
	$(RM) $(OBJECTS) $(DEPS) $(TARGET) $(ATTR_TEST)

$(ATTR_TEST): tests/test_attributes.c $(BUILD_DIR)/attributes.o include/attributes.h | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) tests/test_attributes.c $(BUILD_DIR)/attributes.o -Wl,--wrap=getpwuid,--wrap=getgrgid $(LDLIBS) -o $@

test: $(TARGET) $(ATTR_TEST)
	./$(ATTR_TEST)
	FM_BINARY="$(abspath $(TARGET))" $(PYTHON) tests/test_cli.py
	FM_BINARY="$(abspath $(TARGET))" $(PYTHON) tests/test_terminal.py

-include $(DEPS)
