#!/bin/bash
# start.sh
# Usage: ./start.sh
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

CONFIG_DIR="/etc/redis"
CONFIG_FILE="${CONFIG_DIR}/redis_${IP}_${PORT}.conf"

# Check if the configuration file exists
if [ ! -f "${CONFIG_FILE}" ]; then
    echo "Error: Redis config file for ${IP}:${PORT} does not exist at ${CONFIG_FILE}."
    exit 1
fi

echo "Starting Redis instance using config ${CONFIG_FILE}..."
# Start Redis with the specified configuration
redis-server "${CONFIG_FILE}" &

# Give the server a moment to start
sleep 2

# Verify that Redis is listening on the specified port
if ss -lnt | grep -q ":${PORT} "; then
    echo "Redis instance successfully started on ${IP}:${PORT}."
else
    echo "Error: Redis instance failed to start on ${IP}:${PORT}."
fi
