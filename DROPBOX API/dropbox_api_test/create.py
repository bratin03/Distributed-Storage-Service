import json
import requests

# Load configuration containing the access token
with open('config.json', 'r') as config_file:
    config = json.load(config_file)

access_token = config['access_token']

# Specify the Dropbox file path for simulation
dropbox_path = '/folder/simulated_file.txt'

# Content for the first file upload
file_content1 = b"Hello, Dropbox! This is the first simulated file."

# Content for the second file upload (different content)
file_content2 = b"Hello, Dropbox! This is the second simulated file with different content."

# Dropbox upload endpoint for creating (uploading) a file
url = "https://content.dropboxapi.com/2/files/upload"

def upload_file(path, content, description):
    # Set the headers, including authorization, API arguments, and content type.
    headers = {
        "Authorization": "Bearer " + access_token,
        "Dropbox-API-Arg": json.dumps({
            "path": path,
            "mode": "add",         # "add" mode creates a new file (or auto-renames if it exists)
            "autorename": False,
            "mute": False
        }),
        "Content-Type": "application/octet-stream"
    }
    
    # Make the POST request to the Dropbox API
    response = requests.post(url, headers=headers, data=content)
    
    # Print the response with a description
    print(f"Response for {description}:")
    try:
        response_json = response.json()
        print(json.dumps(response_json, indent=4))
    except json.JSONDecodeError:
        print("Response is not in JSON format:", response.text)
    print("-" * 40)

# (1) First upload: creates the file normally
upload_file(dropbox_path, file_content1, "First upload (creation of file)")

# (2) Second upload: simulates a conflict, triggering autorename behavior with different content
upload_file(dropbox_path, file_content2, "Second upload (simulated autorename)")
