#!/usr/bin/env bash
set -euo pipefail

# 1. Hard-coded path to your JSON config
CONFIG="config/user_config.json"

# 2. Update monitoring_path to use the current $USER
tmpfile=$(mktemp)
jq --arg user "$USER" \
   '.monitoring_path |= sub("/home/[^/]+"; "/home/"+$user)' \
   "$CONFIG" > "$tmpfile"
mv "$tmpfile" "$CONFIG"

# 3. Create the updated monitoring directory
new_mp=$(jq -r '.monitoring_path' "$CONFIG")
mkdir -p "$new_mp"
echo "✔ monitoring_path updated to '$new_mp' (directory created)"

# 4. Grab the primary IP address (first non-loopback)
IP=$(ip route get 1.1.1.1 2>/dev/null \
     | awk '/src/ { print $7; exit }')

# 5. Overwrite device_id with just the IP
tmpfile=$(mktemp)
jq --arg ip "$IP" \
   '.device_id = $ip' \
   "$CONFIG" > "$tmpfile"
mv "$tmpfile" "$CONFIG"

echo "✔ device_id set to '$(jq -r .device_id "$CONFIG")'"
