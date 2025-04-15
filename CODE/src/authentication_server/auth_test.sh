#!/bin/bash

# -------- CONFIG -------- #
SERVER_URL="http://127.0.0.1:30000/login"
USER_ID="lordu"
PASSWORD="test1"
# ------------------------ #

echo "🔐 Logging in to authentication server..."
echo "👉 URL: $SERVER_URL"
echo "👤 User: $USER_ID"

# Prepare JSON payload
JSON_PAYLOAD=$(cat << EOF
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
