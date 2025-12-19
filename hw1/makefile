# ============================================================
# Makefile for HW2 (Dispatcher / Worker pthreads project)
# ============================================================

CC      = gcc
CFLAGS  = -Wall -Wextra -pthread
TARGET  = hw2
SOURCE  = src/main.c src/func.c

# Default target: build the program
all: $(TARGET)

# How to build the program
$(TARGET): $(SOURCE)
	$(CC) $(CFLAGS) $(SOURCE) -o $(TARGET)

# Optional: run with example arguments
run: $(TARGET)
	./$(TARGET) cmdfile.txt 4 10 1

# Clean build artifacts
clean:
	rm -f $(TARGET)