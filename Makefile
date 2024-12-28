# Compiler and flags
CC = gcc
CFLAGS = -Wall -O2
LDFLAGS = -lcurl

# Source and target
SRC = main.c
TARGET = main

# Platform-specific flags
ifeq ($(OS),Windows_NT)
    TARGET := $(TARGET).exe
else
    UNAME_S := $(shell uname -s)
    ifeq ($(UNAME_S),Darwin)
        # macOS specific flags (if needed)
        CFLAGS += -D_DARWIN_C_SOURCE
    else
        # Linux specific flags (if needed)
        CFLAGS += -D_GNU_SOURCE
    endif
endif

# Default rule
all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

# Clean rule
clean:
	rm -f $(TARGET)
