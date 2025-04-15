# Compiler and flags
CXX := g++
CXXFLAGS := -O2 -g -pg -std=c++17 -Wall -I. 

# Output
LIB_NAME := libkv.a
OBJ := kv.o

# Target to build the static library
all: $(LIB_NAME)

$(LIB_NAME): $(OBJ)
	ar rcs $@ $^

kv.o: kv.cpp kv.hpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Clean build files
clean:
	rm -f *.o $(LIB_NAME)
