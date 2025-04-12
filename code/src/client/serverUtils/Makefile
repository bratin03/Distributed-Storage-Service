CXX = g++
CXXFLAGS = -O2 -g -pg -std=c++17 -Wall -Wextra -I../../include
AR = ar
ARFLAGS = rcs

# Source and output paths
SRC_DIR = .
OBJ_DIR = build
TARGET = libserverUtils.a

# Source and object files
SOURCES = $(wildcard $(SRC_DIR)/*.cpp)
OBJECTS = $(patsubst $(SRC_DIR)/%.cpp, $(OBJ_DIR)/%.o, $(SOURCES))

# Default target
all: $(TARGET)

# Static library build
$(TARGET): $(OBJECTS)
	$(AR) $(ARFLAGS) $@ $^

# Compile .cpp to .o
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp | $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Create build directory if it doesn't exist
$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

# Clean up build artifacts
clean:
	rm -rf $(OBJ_DIR) *.a

.PHONY: all clean
