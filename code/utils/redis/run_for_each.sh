#!/bin/bash

# Ensure the script to run is specified
if [ -z "$1" ]; then
  echo "Usage: $0 <script_to_run> <hosts_file>"
  exit 1
fi

SCRIPT_TO_RUN="$1"
HOSTS_FILE="$2"

# Ensure the script to run exists and is executable
if [ ! -x "$SCRIPT_TO_RUN" ]; then
  echo "Error: Script '$SCRIPT_TO_RUN' not found or not executable."
  exit 1
fi

# Ensure the hosts file exists
if [ ! -f "$HOSTS_FILE" ]; then
  echo "Error: Hosts file '$HOSTS_FILE' not found."
  exit 1
fi

# Read each line from the hosts file
while IFS=' ' read -r IP PORT; do
  if [ -n "$IP" ] && [ -n "$PORT" ]; then
    echo "Running '$SCRIPT_TO_RUN' with IP: $IP and Port: $PORT"
    "./$SCRIPT_TO_RUN" "$IP" "$PORT"
  else
    echo "Invalid line in hosts file: $IP $PORT"
  fi
done < "$HOSTS_FILE"
