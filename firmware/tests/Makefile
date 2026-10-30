# Host-side tests for the shared ESP32 protocol and sender code.
#
# These build with any C99 host compiler. No Arduino/ESP-IDF toolchain needed.
#
#   make            # build and run both test binaries
#   make clean

CC ?= cc
CFLAGS ?= -std=c99 -O2 -Wall -Wextra -Wpedantic -Werror
CPPFLAGS ?= -I../shared/src
BUILD_DIR := build
SOURCES := ../shared/src/qos_protocol.c ../shared/src/qos_sender.c
TESTS := $(BUILD_DIR)/test_q1_packet $(BUILD_DIR)/test_sender

.PHONY: all run clean

all: run

$(BUILD_DIR)/test_q1_packet: test_q1_packet.c $(SOURCES) | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ test_q1_packet.c $(SOURCES)

$(BUILD_DIR)/test_sender: test_sender.c $(SOURCES) | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ test_sender.c $(SOURCES)

$(BUILD_DIR):
	mkdir $(BUILD_DIR)

run: $(TESTS)
	./$(BUILD_DIR)/test_q1_packet
	./$(BUILD_DIR)/test_sender

clean:
	rm -rf $(BUILD_DIR)
