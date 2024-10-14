# Default target
.DEFAULT_GOAL := all

# Detect if we are on Windows
ifeq ($(OS),Windows_NT)
    CC = gcc
    CFLAGS = -DWINDOWS -O2 -Wall
    LDFLAGS = -lws2_32 
    EXECUTABLE = udp-forwarder.exe
else
    CC = gcc
    CFLAGS = -O2 -Wall -pthread
    LDFLAGS =
    EXECUTABLE = udp-forwarder
endif

# Source files
SRCS = udp_forwarder.c

# Object files
OBJS = $(SRCS:.c=.o)

# Build target
all: $(EXECUTABLE)

$(EXECUTABLE): $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) -o $@ $(LDFLAGS)

# Compile source files to object files
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Clean up build files
clean:
	rm -f $(OBJS) $(EXECUTABLE)
