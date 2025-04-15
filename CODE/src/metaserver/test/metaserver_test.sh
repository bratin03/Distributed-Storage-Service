#!/bin/bash

# API endpoint
BASE_URL="http://127.0.0.3:30000"   # change this to your server's URL
AUTH_HEADER="Authorization: Bearer eyJhbGciOiJSUzI1NiIsInR5cCI6IkpXVCJ9.eyJleHAiOjE3NDQxMTI2NDgsImlzcyI6ImF1dGgtc2VydmVyIiwic3ViIjoibG9yZHUiLCJ1c2VySUQiOiJsb3JkdSJ9.jKKMgZWiFKA0xRJn_fXOC9dlobiMUvKcsMYPK1Bp7tJTaPdW73tfLd-Q3ne-NjmlNHfK838jJRLnIU3fkwyAXRCnt8k-yWXqhD4iIpsN_R635ktshkVnP7wA7CYEJLH7QREUEgm0tvnC5yj9dpdRdEM8-bXye1CCfiDxzfrfQnjusUu3xpFbbUg27pZoOxKxYCn9ucBNAartfNbdXJxUSP2cJKZvqxgtQFiTLSrirtI8_ZOWNPc_pk2vhV0EpdIi92HelPXaRQwCG_zVb_GFPPgI-h3ILEOu1EkRpvozlvIOZqofMLHJeGVTuHxqBcS0tGI3rGvHrPQau1qzdx6sTA"  # change this after getting token from authentication server
CONTENT_TYPE="Content-Type: application/json"
TEST_FILE="metaserver_test1.json"
RESPONSE_FILE="response.json"

# Send request
HTTP_CODE=$(curl -s -o "$RESPONSE_FILE" -w "%{http_code}" -X POST "$BASE_URL/create-directory" \
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
