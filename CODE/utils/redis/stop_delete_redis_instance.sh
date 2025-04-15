#!/bin/bash
# stop_delete_redis_instance.sh
# Usage: ./stop_delete_redis_instance.sh <IP> <port>
# Example: ./stop_delete_redis_instance.sh 127.0.0.1 6380

if [ "$#" -ne 2 ]; then
  echo "Usage: $0 <IP> <port>"
  exit 1
fi

IP="$1"
PORT="$2"

CONFIG_DIR="/etc/redis"
PID_DIR="/var/run/redis"
LOG_DIR="/var/log/redis"
DATA_DIR="/var/lib/redis"

CONFIG_FILE="${CONFIG_DIR}/redis_${IP}_${PORT}.conf"
PID_FILE="${PID_DIR}/redis_${IP}_${PORT}.pid"
LOG_FILE="${LOG_DIR}/redis_${IP}_${PORT}.log"
DUMP_FILE="${DATA_DIR}/dump_${IP}_${PORT}.rdb"
CLUSTER_CONFIG_FILE="${CONFIG_DIR}/nodes_${IP}_${PORT}.conf"

# Check if the config file exists
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

# Delete the configuration and related files
echo "Deleting config and related files for Redis instance on ${IP}:${PORT}..."

rm -f "${CONFIG_FILE}" "${PID_FILE}" "${LOG_FILE}" "${DUMP_FILE}" "${CLUSTER_CONFIG_FILE}"

echo "Redis instance at ${IP}:${PORT} has been stopped and associated files have been deleted."
