#!/bin/bash

# Compiler settings
CXX=g++
CXXFLAGS="-std=c++20 -Wall -Wextra -pthread -O2"

# Source files
SRC="main.cpp Watcher.cpp"

# Output binary
OUT="watcher_app.out"

# Compilation
$CXX $CXXFLAGS $SRC -o $OUT -linotify-cpp

# Run the compiled binary
echo "Build complete. Run ./$OUT"
