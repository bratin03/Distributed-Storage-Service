import json
import requests

# Load configuration containing the access token
with open('config.json', 'r') as config_file:
    config = json.load(config_file)

access_token = config['access_token']

# Specify the path in Dropbox where you want to create the file
dropbox_path = '/folder/new_file.txt'  # change as needed

# Specify the content for the new file
file_content = b"Hello, Dropbox! This is a new file created via the API."

# Dropbox upload endpoint for creating (uploading) a file
url = "https://content.dropboxapi.com/2/files/upload"

# Set the headers, including authorization, API arguments, and content type.
headers = {
    "Authorization": "Bearer " + access_token,
    "Dropbox-API-Arg": json.dumps({
        "path": dropbox_path,
        "mode": "add",         # "add" mode creates a new file (or auto-renames if it exists)
        "autorename": True,
        "mute": False
    }),
    "Content-Type": "application/octet-stream"
}

# Make the POST request to the Dropbox API
response = requests.post(url, headers=headers, data=file_content)

# Properly print the JSON response with indentation
try:
    response_json = response.json()
    print(json.dumps(response_json, indent=4))
except json.JSONDecodeError:
    print("Response is not in JSON format:", response.text)
