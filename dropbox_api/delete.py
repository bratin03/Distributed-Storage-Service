import json
import requests

# Load configuration containing the access token
with open("config.json", "r") as config_file:
    config = json.load(config_file)

access_token = config["access_token"]

# Dropbox API endpoint for deleting a file or folder
url = "https://api.dropboxapi.com/2/files/delete_v2"

# Set up the request headers
headers = {
    "Authorization": "Bearer " + access_token,
    "Content-Type": "application/json",
}

# Define the JSON payload.
# Replace '/path/to/your/directory' with the actual path of the directory you want to delete.
payload = {
    "path": "/A"
}

# Make the POST request to the Dropbox API
response = requests.post(url, headers=headers, data=json.dumps(payload))

# Parse and print the JSON response
if response.status_code == 200:
    deletion_result = response.json()
    print(json.dumps(deletion_result, indent=4))
else:
    print(f"Error: {response.status_code}")
    print(response.text)
