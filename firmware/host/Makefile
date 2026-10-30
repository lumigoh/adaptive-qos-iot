# Host UDP sender for local protocol testing.
#
#   make        # builds build/qos_sender_host
#   make clean
#
# This is a development tool, not ESP32 firmware.

CC ?= cc
CFLAGS ?= -std=c99 -O2 -Wall -Wextra -Wpedantic -Werror
CPPFLAGS ?= -I../shared/src
BUILD_DIR := build
TARGET := $(BUILD_DIR)/qos_sender_host
SOURCES := ../shared/src/qos_protocol.c ../shared/src/qos_sender.c \
           ../shared/src/qos_udp.c
ifeq ($(OS),Windows_NT)
LDLIBS += -lws2_32
endif

.PHONY: all clean

all: $(TARGET)

$(TARGET): main.c $(SOURCES) | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ main.c $(SOURCES) $(LDLIBS)

$(BUILD_DIR):
	mkdir $(BUILD_DIR)

clean:
	rm -rf $(BUILD_DIR)
