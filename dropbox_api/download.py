import json
import requests

# Load configuration containing the access token
with open("config.json", "r") as config_file:
    config = json.load(config_file)

access_token = config["access_token"]

# Specify the Dropbox file path to download
dropbox_file_path = "/folder/file.txt"  # change as needed

# Dropbox download endpoint URL
url = "https://content.dropboxapi.com/2/files/download"

# Set the required headers.
# The 'Dropbox-API-Arg' header contains the JSON-encoded path of the file.
headers = {
    "Authorization": "Bearer " + access_token,
    "Dropbox-API-Arg": json.dumps({"path": dropbox_file_path}),
}

# Make the POST request to download the file
response = requests.post(url, headers=headers)

# The file metadata is in the 'dropbox-api-result' response header.
metadata = json.loads(response.headers.get("dropbox-api-result"))
# The file content (binary data) is in the response body.
file_content = response.content

# Print the file metadata
print("Metadata:", metadata)

# Optionally, save the file to disk
with open("downloaded_file.txt", "wb") as f:
    f.write(file_content)
