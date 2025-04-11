# Compiler and flags
CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -O2 -g -pg -I.
AR = ar

# Source files
SRC = load_config.cpp
OBJ = $(SRC:.cpp=.o)
LIB = libload_config.a

# Default target
all: $(LIB)

# Rule to build static library
$(LIB): $(OBJ)
	$(AR) rcs $@ $^

# Rule to compile source files to object files
%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Clean up object and lib files
clean:
	rm -f $(OBJ) $(LIB)

.PHONY: all clean
