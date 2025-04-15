import json
import requests

# Load configuration containing the access token
with open('config.json', 'r') as config_file:
    config = json.load(config_file)

access_token = config['access_token']

# Dropbox API endpoint for adding a property template
url = "https://api.dropboxapi.com/2/file_properties/templates/add"

headers = {
    "Authorization": "Bearer " + access_token,
    "Content-Type": "application/json"
}

# Define the property template payload with a field "version_number"
payload = {
    "name": "File Version Template", 
    "description": "Template for file version number",
    "fields": [
        {
            "name": "version_number",
            "description": "Version number of the file",
            "type": "string"  # You can also use "number" if needed
        }
    ]
}

response = requests.post(url, headers=headers, data=json.dumps(payload))

print("Status Code:", response.status_code)
print("Response Text:", response.text)

try:
    template_response = response.json()
    print("Template Response:", template_response)
except ValueError as e:
    print("Error parsing JSON response:", e)

