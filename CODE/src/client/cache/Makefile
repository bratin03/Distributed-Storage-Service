# Compiler and flags
CXX := g++
CXXFLAGS := -std=c++17 -Wall -Wextra -O2 -pthread -Wextra -g -pg -I.

# File names
LIB_NAME := libcache.a
TARGET := main.out

# Source files
LIB_SRC := cache.cpp
LIB_OBJ := $(LIB_SRC:.cpp=.o)

MAIN_SRC := main.cpp
MAIN_OBJ := $(MAIN_SRC:.cpp=.o)

# Build static library and main
all: $(LIB_NAME) $(TARGET)

# Build static library
$(LIB_NAME): $(LIB_OBJ)
	ar rcs $@ $^

# Build main and link with static library
$(TARGET): $(MAIN_OBJ) $(LIB_NAME)
	$(CXX) $(CXXFLAGS) -o $@ $^ 

# Clean up object files, library, and binary
clean:
	rm -f *.o $(LIB_NAME) $(TARGET)

.PHONY: all clean
