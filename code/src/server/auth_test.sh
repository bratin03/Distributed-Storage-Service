#!/bin/bash

# -------- CONFIG -------- #
SERVER_URL="http://127.0.1.1/10000"
USER_ID="lordu"
PASSWORD="testpassword"
# ------------------------ #

echo "🔐 Logging in to authentication server..."
echo "👉 URL: $SERVER_URL"
echo "👤 User: $USER_ID"

# Prepare JSON payload
JSON_PAYLOAD=$(cat <<EOF
{
    "userID": "$USER_ID",
    "password": "$PASSWORD"
}
EOF
)

# Send POST request
RESPONSE=$(curl -s -X POST "$SERVER_URL" \
    -H "Content-Type: application/json" \
    -d "$JSON_PAYLOAD")

echo "🔍 Response from server:
$RESPONSE"
