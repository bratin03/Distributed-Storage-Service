#!/bin/bash
# create.sh
# Usage: ./create.sh
# host.txt must contain a line: <IP> <port>

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

# Check if redis-server is installed; if not, install it.
if ! command -v redis-server >/dev/null 2>&1; then
    echo "redis-server not found. Installing..."
    sudo apt-get update && sudo apt-get install -y redis-server
    if ! command -v redis-server >/dev/null 2>&1; then
        echo "Failed to install redis-server. Exiting."
        exit 1
    fi
fi

CONFIG_DIR="/etc/redis"
BASE_CONFIG="${CONFIG_DIR}/redis.conf"
NEW_CONFIG="${CONFIG_DIR}/redis_${IP}_${PORT}.conf"

# Check if config already exists
if [ -f "${NEW_CONFIG}" ]; then
    echo "Error: Redis config for ${IP}:${PORT} already exists at ${NEW_CONFIG}."
    exit 1
fi

# Ensure the base config exists
if [ ! -f "${BASE_CONFIG}" ]; then
    echo "Error: Base config file not found at ${BASE_CONFIG}."
    exit 1
fi

# Create the new config by copying the base one
cp "${BASE_CONFIG}" "${NEW_CONFIG}"
echo "Creating new Redis config at ${NEW_CONFIG} ..."

# Update the config:
sed -i "s/^port .*/port ${PORT}/" "${NEW_CONFIG}"
if grep -q "^bind" "${NEW_CONFIG}"; then
    sed -i "s/^bind .*/bind ${IP}/" "${NEW_CONFIG}"
else
    echo "bind ${IP}" >>"${NEW_CONFIG}"
fi
sed -i "s|^pidfile .*|pidfile /var/run/redis/redis_${IP}_${PORT}.pid|" "${NEW_CONFIG}"
sed -i "s|^logfile .*|logfile /var/log/redis/redis_${IP}_${PORT}.log|" "${NEW_CONFIG}"
sed -i "s/^dbfilename .*/dbfilename dump_${IP}_${PORT}.rdb/" "${NEW_CONFIG}"

if grep -q "^cluster-config-file" "${NEW_CONFIG}"; then
    sed -i "s|^cluster-config-file .*|cluster-config-file nodes_${IP}_${PORT}.conf|" "${NEW_CONFIG}"
fi

echo "Redis config file created at ${NEW_CONFIG}."
