#!/bin/bash
set -x
CXX=g++
CXXFLAGS="-O2 -std=c++17"
SOURCES="metadata.cpp"
OBJECTS="metadata.o"
$CXX $CXXFLAGS -c $SOURCES -o metadata.o
ar rcs libmetadata.a metadata.o
echo "Static library libmetadata.a created successfully."
