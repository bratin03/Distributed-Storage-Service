#!/bin/bash

# Check if clang-format is installed
if ! command -v clang-format &> /dev/null; then
    echo "clang-format could not be found. Please install it."
    exit 1
fi

# Get list of modified, added, or untracked C++ files
FILES=$(git status --porcelain | grep -E '(\.cpp|\.hpp|\.h|\.c|\.tpp|\.cc|\.cxx|\.hxx|\.ixx|\.inl)$' | awk '{print $2}')

if [ -z "$FILES" ]; then
    echo "No modified C++ files found."
    exit 0
fi

echo "Formatting C++ files..."
echo "$FILES" | xargs clang-format -i -style=file
echo "Done."
