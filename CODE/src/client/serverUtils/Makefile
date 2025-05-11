CXX = g++
CXXFLAGS = -O2 -g -pg -std=c++17 -Wall -Wextra -I../../include
AR = ar
ARFLAGS = rcs

# Source and output paths
SRC_DIR = .
TARGET = libserverUtils.a

# Source and object files
SOURCES = $(wildcard $(SRC_DIR)/*.cpp)
OBJECTS = $(SOURCES:.cpp=.o)

# Default target
all: $(TARGET)

# Static library build
$(TARGET): $(OBJECTS)
	$(AR) $(ARFLAGS) $@ $^

# Compile .cpp to .o
%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Clean up build artifacts
clean:
	rm -f *.o *.a

.PHONY: all clean
