import json
import requests

# Load configuration containing the access token
with open("config.json", "r") as config_file:
    config = json.load(config_file)

access_token = config["access_token"]

# Dropbox API endpoint for listing folder contents
url = "https://api.dropboxapi.com/2/files/list_folder"

# Set up the request headers
headers = {
    "Authorization": "Bearer " + access_token,
    "Content-Type": "application/json",
}

# Define the JSON payload. In this case, we list the root folder.
payload = {
    "path": "",  # Set to "" for the root folder or specify a subfolder path
    "recursive": False,  # Set to True to list contents of subfolders as well
    "include_media_info": False,
    "include_deleted": False,
    "include_has_explicit_shared_members": False,
    "include_mounted_folders": True,
}

# Make the POST request to the Dropbox API
response = requests.post(url, headers=headers, data=json.dumps(payload))

# Parse and print the JSON response
if response.status_code == 200:
    folder_contents = response.json()
    print(json.dumps(folder_contents, indent=4))
else:
    print(f"Error: {response.status_code}")
    print(response.text)