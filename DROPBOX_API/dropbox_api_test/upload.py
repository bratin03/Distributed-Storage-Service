import json
import requests

# Load configuration containing the access token
with open('config.json', 'r') as config_file:
    config = json.load(config_file)

access_token = config['access_token']

# Specify the path in Dropbox where you want to upload the file
dropbox_path = '/folder/file.txt'  # change as needed

# Specify the expected current revision of the file on Dropbox
expected_revision = "0163162083d101e00000002ca83cbb3"  # e.g., "a1c10ce0dd78"

# Specify the local file you want to upload
local_file = 'local_file.txt'

# Read the file content to upload
with open(local_file, 'rb') as f:
    file_data = f.read()

# Dropbox upload endpoint
url = "https://content.dropboxapi.com/2/files/upload"

# Set the headers, including authorization, API arguments, and content type.
headers = {
    "Authorization": "Bearer " + access_token,
    "Dropbox-API-Arg": json.dumps({
        "path": dropbox_path,
        "mode": {".tag": "update", "update": expected_revision},
        "autorename": False,  # In update mode, autorename is typically disabled.
        "mute": False
    }),
    "Content-Type": "application/octet-stream"
}

# Make the POST request to the Dropbox API
response = requests.post(url, headers=headers, data=file_data)

# Properly print the JSON response with indentation
try:
    response_json = response.json()
    print(json.dumps(response_json, indent=4))
except json.JSONDecodeError:
    print("Response is not in JSON format:", response.text)
