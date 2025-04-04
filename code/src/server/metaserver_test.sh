#!/bin/bash

# API endpoint
BASE_URL="http://localhost:<port>"
AUTH_HEADER="Authorization: Basic dXNlcjE6cGFzczEyMzQ="  # change this after getting token from authentication server
CONTENT_TYPE="Content-Type: application/json"
TEST_FILE="metaserver_test1.json"
RESPONSE_FILE="response.json"

# Send request
HTTP_CODE=$(curl -s -o "$RESPONSE_FILE" -w "%{http_code}" -X POST "$BASE_URL/create_directory" \
    -H "$CONTENT_TYPE" \
    -H "$AUTH_HEADER" \
    -d @"$TEST_FILE")

# Check if response.json was created and is not empty
if [[ ! -s "$RESPONSE_FILE" ]]; then
    echo "❌ Error: No response received or empty response file!"
    exit 1
fi

# Read JSON error message if any
ERROR_MSG=$(jq -r '.error // empty' "$RESPONSE_FILE")

# Handle different responses
if [[ $HTTP_CODE -eq 200 ]]; then
    echo "✅ Directory created successfully!"
    jq '.' "$RESPONSE_FILE"  # Pretty-print JSON response
elif [[ "$ERROR_MSG" == "Directory already exists" ]]; then
    echo "⚠️ Directory already exists."
elif [[ "$ERROR_MSG" == "Parent directory not found" ]]; then
    echo "❌ Error: Parent directory not found."
else
    echo "❌ Unexpected error: $(cat "$RESPONSE_FILE")"
fi

# Clean up
rm "$RESPONSE_FILE"
