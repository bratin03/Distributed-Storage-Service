#!/bin/sh
# delete_leveldb.sh - deletes the specified LevelDB directory
#
# Usage: ./delete_leveldb.sh <leveldb_directory>

if [ "$#" -ne 1 ]; then
    echo "Usage: $0 <leveldb_directory>"
    exit 1
fi

LEVELDB_DIR="$1"

if [ ! -d "$LEVELDB_DIR" ]; then
    echo "Error: Directory '$LEVELDB_DIR' does not exist."
    exit 1
fi

echo "Deleting LevelDB directory: $LEVELDB_DIR"
rm -rf "$LEVELDB_DIR"

if [ $? -eq 0 ]; then
    echo "Directory '$LEVELDB_DIR' deleted successfully."
    exit 0
else
    echo "Error: Failed to delete directory '$LEVELDB_DIR'."
    exit 1
fi
