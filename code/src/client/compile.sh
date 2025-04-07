#!/bin/bash

# ===== CONFIGURATION =====
SRC_DIR="."
BUILD_DIR="."
TARGET="client.out"

SRC_FILES="$SRC_DIR/client.cpp"    
INCLUDE_PATHS="-I./load_config -I./login"      
LIB_PATHS="-L./load_config -L./login"       
LIBS="-lm -lpthread -llogin -lload_config"
CXXFLAGS="-Wall -O2 -std=c++17 -Wextra" 

# ===== BUILD PROCESS =====
echo "Compiling C++ program..."

# Create build directory if it doesn't exist
mkdir -p "$BUILD_DIR"

# Compile
g++ $CXXFLAGS $INCLUDE_PATHS $SRC_FILES $LIB_PATHS $LIBS -o "$BUILD_DIR/$TARGET"

# Check result
if [ $? -eq 0 ]; then
    echo "Build succeeded: $BUILD_DIR/$TARGET"
else
    echo "Build failed"
fi
