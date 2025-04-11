# Makefile to create static library liblogin.a from login.cpp

# Compiler and archiver
CXX = g++
AR = ar

# Flags
CXXFLAGS = -Wall -Wextra -O2 -std=c++17 -g -pg -I.
ARFLAGS = rcs

# Source and output
SRC = login.cpp
OBJ = login.o
LIB = liblogin.a

# Default target
all: $(LIB)

# Create object file
$(OBJ): $(SRC)
	$(CXX) $(CXXFLAGS) -c $(SRC) -o $(OBJ)

# Create static library
$(LIB): $(OBJ)
	$(AR) $(ARFLAGS) $(LIB) $(OBJ)

# Clean build artifacts
clean:
	rm -f $(OBJ) $(LIB)

.PHONY: all clean
