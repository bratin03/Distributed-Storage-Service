#!/bin/bash

# API endpoint
BASE_URL="http://127.0.0.1:35000"   # change this to your server's URL
AUTH_HEADER="Authorization: Bearer eyJhbGciOiJSUzI1NiIsInR5cCI6IkpXVCJ9.eyJleHAiOjE3NDM4MzM5MzcsImlzcyI6ImF1dGgtc2VydmVyIiwic3ViIjoidXNlcjEiLCJ1c2VySUQiOiJ1c2VyMSJ9.JXh-isDZ-XQBZJsJOEhUqmAfjA2cOvTKeLu0XJgPgchiszmfX9X5-wzJoQKvG0axGzD9UW9QBE5SpiX76wK8XCCtffPdOJnVV70yREEZcwi7rt4PK63CDXZbKDiwWDZFPY8zHlkB7gvuAwilvftnwtLseTkqSTittKXgObDRohn9tG9dmhiRJGKE9SOKQns1G9BEHLYdx6iQZz57_YVhHn_gX8cpbYX__GXmjTaOkZj3U2_LA4JgzugjkJGneg-7l8LOnfRwTpqIR_p70cQYm-7-QMyH2YLGGg4sdzk2V3HaZwd7EecZ5GsBI1whw_0NJP_IxNp420hvNIWlNIswlQ"  # change this after getting token from authentication server
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
