import json
import requests

# Load configuration containing the access token
with open("config.json", "r") as config_file:
    config = json.load(config_file)

access_token = config["access_token"]

# Specify the Dropbox file path to download
dropbox_file_path = "/B/x.txt"  # change as needed

# Dropbox download endpoint URL
url = "https://content.dropboxapi.com/2/files/download"

# Set the required headers.
headers = {
    "Authorization": "Bearer " + access_token,
    "Dropbox-API-Arg": json.dumps({"path": dropbox_file_path}),
}

try:
    # Make the POST request to download the file
    response = requests.post(url, headers=headers)

    # Check if the response is successful
    if response.status_code == 200:
        # Parse file metadata from the header "Dropbox-API-Result"
        metadata = json.loads(response.headers["Dropbox-API-Result"])
        print("Metadata:")
        print(json.dumps(metadata, indent=4))

        # Save the downloaded file content to a local file
        with open("downloaded_file.txt", "wb") as f:
            f.write(response.content)
        print("File downloaded successfully.")
    else:
        print(f"Error: {response.status_code} - {response.text}")

except Exception as e:
    print("An error occurred:", e)
