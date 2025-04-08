#!/bin/bash
# stop.sh
# Usage: ./stop.sh
# host.txt must contain a line: <IP> <port>

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

CONFIG_FILE="${CONFIG_DIR}/redis_${IP}_${PORT}.conf"
PID_FILE="${PID_DIR}/redis_${IP}_${PORT}.pid"

# Check if the configuration file exists
if [ ! -f "${CONFIG_FILE}" ]; then
  echo "Error: Redis config for ${IP}:${PORT} does not exist at ${CONFIG_FILE}."
  exit 1
fi

echo "Attempting to shutdown Redis instance at ${IP}:${PORT}..."

# Attempt graceful shutdown via redis-cli
redis-cli -h "${IP}" -p "${PORT}" shutdown >/dev/null 2>&1

# Allow a few seconds for shutdown
sleep 2

# If the instance is still running, try to kill it using the PID file
if ss -lnt | grep -q ":${PORT} "; then
  echo "Redis instance on port ${PORT} did not shutdown gracefully."
  if [ -f "${PID_FILE}" ]; then
    PID=$(cat "${PID_FILE}")
    echo "Killing process with PID ${PID}..."
    kill -9 "${PID}"
    sleep 1
    if ss -lnt | grep -q ":${PORT} "; then
      echo "Error: Unable to stop Redis instance on ${IP}:${PORT}."
      exit 1
    else
      echo "Redis instance forcefully stopped."
    fi
  else
    echo "Error: PID file not found; cannot kill process."
    exit 1
  fi
else
  echo "Redis instance on ${IP}:${PORT} has been shutdown gracefully."
fi

echo "Redis instance at ${IP}:${PORT} has been stopped."
