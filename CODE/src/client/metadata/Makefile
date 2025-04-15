# Compiler and flags
CXX := g++
CXXFLAGS := -Wall -Wextra -O2 -std=c++17 -I. -g -pg

# RocksDB and JSON dependencies
INCLUDES := -I/usr/include -I/usr/local/include
LIBS := -L/usr/lib -L/usr/local/lib -lrocksdb

# Source files
SRCS := metadata.cpp
OBJS := $(SRCS:.cpp=.o)

# Target static library
TARGET := libmetadata.a

# Default rule
all: $(TARGET)

# Build static library
$(TARGET): $(OBJS)
	ar rcs $@ $^

# Object file rule
%.o: %.cpp metadata.hpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

# Clean rule
clean:
	rm -f $(OBJS) $(TARGET)
