# Makefile for building merge as a static library

# Compiler and flags
CXX = g++
CXXFLAGS = -std=c++17 -O2 -Wall -I. -Wextra -g -pg

# Output
TARGET = libmerge.a

# Sources and objects
SRCS = merge.cpp
OBJS = $(SRCS:.cpp=.o)

# Default target
all: $(TARGET)

# Rule to build static library
$(TARGET): $(OBJS)
	ar rcs $@ $^

# Clean build artifacts
clean:
	rm -f $(OBJS) $(TARGET)

# Phony targets
.PHONY: all clean
