#!/bin/bash
# redis_client.sh
# Usage: ./redis_client.sh <IP> <port>
# Example: ./redis_client.sh 127.0.0.1 6379

if [ "$#" -ne 2 ]; then
    echo "Usage: $0 <IP> <port>"
    exit 1
fi

IP="$1"
PORT="$2"

echo "Connecting to Redis server at ${IP}:${PORT}..."
# Launch the interactive Redis CLI session
redis-cli -h "${IP}" -p "${PORT}"
