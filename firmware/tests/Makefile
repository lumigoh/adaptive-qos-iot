# Host-side tests for the shared ESP32 protocol code.
#
# These build with any C99 host compiler. No Arduino/ESP-IDF toolchain needed.
#
#   make            # build and run
#   make clean

CC ?= cc
CFLAGS ?= -std=c99 -O2 -Wall -Wextra -Wpedantic -Werror
CPPFLAGS ?= -I../shared
BUILD_DIR := build
TARGET := $(BUILD_DIR)/test_q1_packet
SOURCES := test_q1_packet.c ../shared/qos_protocol.c

.PHONY: all run clean

all: run

$(TARGET): $(SOURCES) ../shared/qos_protocol.h | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ $(SOURCES)

$(BUILD_DIR):
	mkdir $(BUILD_DIR)

run: $(TARGET)
	./$(TARGET)

clean:
	rm -rf $(BUILD_DIR)
