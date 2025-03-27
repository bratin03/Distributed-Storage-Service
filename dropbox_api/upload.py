import json
import requests

# Load configuration containing the access token
with open('config.json', 'r') as config_file:
    config = json.load(config_file)

access_token = config['access_token']

# Specify the path in Dropbox where you want to upload the file
dropbox_path = '/folder/file.txt'  # change as needed

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
        "mode": "add",         # can be "add" (default), "overwrite", or "update"
        "autorename": True,
        "mute": False
    }),
    "Content-Type": "application/octet-stream"
}

# Make the POST request to the Dropbox API
response = requests.post(url, headers=headers, data=file_data)

# Print the JSON response
print(response.json())
