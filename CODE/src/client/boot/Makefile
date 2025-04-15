# boot/Makefile

CXX = g++
CXXFLAGS = -Wall -O2  -g -pg -I..

# Source and object files
SRC = boot.cpp
OBJ = boot.o
LIB = libboot.a

# Default target
all: $(LIB)

# Build static library from object file
$(LIB): $(OBJ)
	ar rcs $@ $^

# Compile source to object file
$(OBJ): $(SRC)
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Clean target
clean:
	rm -f $(OBJ) $(LIB)

.PHONY: all clean
