# GNP-based Association Rule Mining System Makefile
# Compiles main.c which requires math library

CC = gcc
CFLAGS = -Wall -g -O2
LDFLAGS = -lm
TARGET = main
SOURCE = main.c

# Default target
all: $(TARGET)

# Build main executable
$(TARGET): $(SOURCE)
	$(CC) $(CFLAGS) -o $(TARGET) $(SOURCE) $(LDFLAGS)

# Run the program (process all 20 forex pairs)
run: $(TARGET)
	./$(TARGET) --all

# Clean build artifacts
clean:
	rm -f $(TARGET)

# Create output directories
setup:
	mkdir -p output/IA output/IB output/pool output/doc

# Full build and setup
install: clean $(TARGET) setup

.PHONY: all run clean setup install