#!/bin/bash
# start_redis_instance.sh
# Usage: ./start_redis_instance.sh <IP> <port>
# Example: ./start_redis_instance.sh 127.0.0.1 6380

if [ "$#" -ne 2 ]; then
  echo "Usage: $0 <IP> <port>"
  exit 1
fi

IP="$1"
PORT="$2"

CONFIG_DIR="/etc/redis"
CONFIG_FILE="${CONFIG_DIR}/redis_${IP}_${PORT}.conf"

# Check if the configuration file exists
if [ ! -f "${CONFIG_FILE}" ]; then
  echo "Error: Redis config file for ${IP}:${PORT} does not exist at ${CONFIG_FILE}."
  exit 1
fi

echo "Starting Redis instance using config ${CONFIG_FILE}..."
# Start Redis with the specified configuration. (This script does NOT create a systemd service.)
redis-server "${CONFIG_FILE}" &

# Give the server a moment to start
sleep 2

# Verify that Redis is listening on the specified port
if ss -lnt | grep -q ":${PORT} "; then
  echo "Redis instance successfully started on ${IP}:${PORT}."
else
  echo "Error: Redis instance failed to start on ${IP}:${PORT}."
fi
