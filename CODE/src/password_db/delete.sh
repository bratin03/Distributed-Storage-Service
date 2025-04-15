#!/bin/bash
# delete.sh
# Usage: ./delete.sh
# host.txt must contain a line: <IP> <port>
# This script deletes associated Redis configuration and related files for a specific instance (assume instance stopped).

HOST_FILE="host.txt"

if [ ! -f "$HOST_FILE" ]; then
  echo "Error: $HOST_FILE not found."
  exit 1
fi

read -r IP PORT < "$HOST_FILE"

if [ -z "$IP" ] || [ -z "$PORT" ]; then
  echo "Error: Invalid content in $HOST_FILE. Expected format: <IP> <port>"
  exit 1
fi

CONFIG_DIR="/etc/redis"
PID_DIR="/var/run/redis"
LOG_DIR="/var/log/redis"
DATA_DIR="/var/lib/redis"

CONFIG_FILE="${CONFIG_DIR}/redis_${IP}_${PORT}.conf"
PID_FILE="${PID_DIR}/redis_${IP}_${PORT}.pid"
LOG_FILE="${LOG_DIR}/redis_${IP}_${PORT}.log"
DUMP_FILE="${DATA_DIR}/dump_${IP}_${PORT}.rdb"

echo "Deleting associated Redis files for ${IP}:${PORT}..."

for file in "${CONFIG_FILE}" "${PID_FILE}" "${LOG_FILE}" "${DUMP_FILE}"; do
  if [ -f "$file" ]; then
    rm -f "$file"
    echo "Deleted $file"
  else
    echo "File not found: $file"
  fi
done

echo
