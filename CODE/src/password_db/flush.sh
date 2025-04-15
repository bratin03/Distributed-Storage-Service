#!/bin/bash
# flush.sh
# Usage: ./flush.sh
# Requires host.txt with: <IP> <port>

HOST_FILE="host.txt"

if [ ! -f "$HOST_FILE" ]; then
    echo "Error: $HOST_FILE not found."
    exit 1
fi

read -r IP PORT <"$HOST_FILE"

if [ -z "$IP" ] || [ -z "$PORT" ]; then
    echo "Error: Invalid content in $HOST_FILE. Expected format: <IP> <port>"
    exit 1
fi

echo "Flushing Redis at ${IP}:${PORT}..."
redis-cli -h "$IP" -p "$PORT" FLUSHALL

if [ $? -eq 0 ]; then
    echo "Redis at ${IP}:${PORT} successfully flushed."
else
    echo "Error: Failed to flush Redis at ${IP}:${PORT}."
    exit 1
fi
