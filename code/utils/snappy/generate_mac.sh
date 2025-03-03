#!/bin/bash
# Output file name
output="/tmp/a.txt"
rm -f "$output"
# Target file size in bytes (10 MB = 10*1024*1024)
target_size=$((10 * 1024 * 1024))
# Define the alphabet pool (capital and small letters)
alphabet="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz"

# Truncate/create the output file
: > "$output"

# Loop until file size is at least 10MB
while [ $(stat -f%z "$output") -lt $target_size ]; do
    # Pick a random index from 0 to 51 (there are 52 letters)
    index=$(( RANDOM % ${#alphabet} ))
    # Extract the random letter
    char="${alphabet:$index:1}"
    # Generate a line with 65536 copies of that character.
    line=$(printf "%.0s$char" {1..65536})
    # Append the line to the file followed by a newline.
    echo "$line" >> "$output"
done
