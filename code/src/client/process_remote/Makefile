# Makefile for building static library from process_remote

# Compiler and flags
CXX := g++
CXXFLAGS := -Wall -Wextra -O2 -std=c++17 -g -pg

# Targets and files
TARGET := libprocess_remote.a
SRC := process_remote.cpp
OBJ := $(SRC:.cpp=.o)
HEADER := process_remote.hpp

# Default target
all: $(TARGET)

# Static library creation
$(TARGET): $(OBJ)
	ar rcs $@ $^

# Compile object file
%.o: %.cpp $(HEADER)
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Clean build files
clean:
	rm -f $(OBJ) $(TARGET)

.PHONY: all clean
