# Makefile for static library libprocess_local.a

CXX := g++
CXXFLAGS := -O2 -g -pg -Wall -I.

SRC := process_local.cpp
OBJ := $(SRC:.cpp=.o)
TARGET := libprocess_local.a

all: $(TARGET)

$(TARGET): $(OBJ)
	ar rcs $@ $^

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(OBJ) $(TARGET)
