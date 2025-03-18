#!/bin/bash
# create_redis_instance.sh
# Usage: ./create_redis_instance.sh <IP> <port>
# Example: ./create_redis_instance.sh 127.0.0.1 6380

if [ "$#" -ne 2 ]; then
    echo "Usage: $0 <IP> <port>"
    exit 1
fi

IP="$1"
PORT="$2"

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
# Change port
sed -i "s/^port .*/port ${PORT}/" "${NEW_CONFIG}"
# Change the bind address
if grep -q "^bind" "${NEW_CONFIG}"; then
    sed -i "s/^bind .*/bind ${IP}/" "${NEW_CONFIG}"
else
    echo "bind ${IP}" >>"${NEW_CONFIG}"
fi
# Update pidfile, logfile and dbfilename to include IP and port so they don’t conflict
sed -i "s|^pidfile .*|pidfile /var/run/redis/redis_${IP}_${PORT}.pid|" "${NEW_CONFIG}"
sed -i "s|^logfile .*|logfile /var/log/redis/redis_${IP}_${PORT}.log|" "${NEW_CONFIG}"
sed -i "s/^dbfilename .*/dbfilename dump_${IP}_${PORT}.rdb/" "${NEW_CONFIG}"

# Optionally update the cluster-config-file if present
if grep -q "^cluster-config-file" "${NEW_CONFIG}"; then
    sed -i "s|^cluster-config-file .*|cluster-config-file nodes_${IP}_${PORT}.conf|" "${NEW_CONFIG}"
fi

echo "Redis config file created at ${NEW_CONFIG}."
