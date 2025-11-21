#!/bin/bash

# Check if black is installed
if ! command -v black &> /dev/null; then
    echo "black could not be found. Please install it."
    exit 1
fi

# Check if isort is installed
if ! command -v isort &> /dev/null; then
    echo "isort could not be found. Please install it."
    exit 1
fi

# Get list of modified or new Python files
FILES=$(git status --porcelain | grep -E '.*\.py$' | awk '{print $2}')

if [ -z "$FILES" ]; then
    echo "No modified Python files found."
    exit 0
fi

echo "Formatting Python files..."
echo "$FILES" | xargs black
echo "$FILES" | xargs isort
echo "Done."
