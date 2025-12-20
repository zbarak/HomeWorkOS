# ============================================================
# Makefile for HW2 (Dispatcher / Worker pthreads project)
# ============================================================

CC      = gcc
CFLAGS  = -Wall -Wextra -pthread -g -fanalyzer -fsanitize=address
TARGET  = hw2
SOURCE  = src/main.c src/func.c

# Default target: build the program
all: $(TARGET)

# How to build the program
$(TARGET): $(SOURCE)
	$(CC) $(CFLAGS) $(SOURCE) -o $(TARGET)

# Optional: run with example arguments
run: $(TARGET)
	./$(TARGET) cmdfile.txt 3 3 1

# Clean build artifacts
clean:
	rm -f $(TARGET)

clean-all:
	@rm -f $(TARGET)
	@rm -f thread*.txt stats.txt dispatcher.txt count*.txt