import json
import requests

# Load configuration containing the access token
with open('config.json', 'r') as config_file:
    config = json.load(config_file)

access_token = config['access_token']

# Dropbox API endpoint for getting file metadata
url = "https://api.dropboxapi.com/2/files/get_metadata"

# Set up the request headers
headers = {
    "Authorization": "Bearer " + access_token,
    "Content-Type": "application/json"
}

# Define the payload without include_property_groups if not needed
payload = {
    "path": "/B",  # The file path
    "include_media_info": True,
    "include_deleted": False,
    "include_has_explicit_shared_members": True
    # Omit "include_property_groups" if you don't have any property groups to request
}

# Make the POST request to get the file metadata
response = requests.post(url, headers=headers, data=json.dumps(payload))

# Print the JSON response containing the extended metadata
print(response.json())
