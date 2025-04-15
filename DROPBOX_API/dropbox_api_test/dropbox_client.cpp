#include "dropbox_client.h"
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>

// Constructor: Read the config file to obtain the access token and initialize CURL.
DropboxClient::DropboxClient(const std::string &configPath)
{
    // Open and parse the JSON configuration file.
    std::ifstream config_file(configPath);
    if (!config_file.is_open())
    {
        throw std::runtime_error("Failed to open config file: " + configPath);
    }
    nlohmann::json config;
    try
    {
        config = nlohmann::json::parse(config_file);
    }
    catch (const nlohmann::json::parse_error &e)
    {
        throw std::runtime_error("JSON parsing error: " + std::string(e.what()));
    }
    if (!config.contains("access_token"))
    {
        throw std::runtime_error("access_token not found in config file");
    }
    accessToken = config["access_token"];

    // Initialize CURL.
    curl_global_init(CURL_GLOBAL_ALL);
    curlHandle = curl_easy_init();
    if (!curlHandle)
    {
        throw std::runtime_error("Failed to initialize CURL");
    }
}

// Destructor: Cleanup CURL.
DropboxClient::~DropboxClient()
{
    if (curlHandle)
    {
        curl_easy_cleanup(curlHandle);
    }
    curl_global_cleanup();
}

// A helper function that sets up CURL with the provided parameters, performs the request,
// and fills in a DropboxResponse struct.
DropboxResponse DropboxClient::performRequest(const std::string &url,
                                              const std::string &args,
                                              const std::string &data,
                                              struct curl_slist *headers)
{
    DropboxResponse response;
    std::string readBuffer;

    // Set the URL.
    curl_easy_setopt(curlHandle, CURLOPT_URL, url.c_str());
    // Set headers.
    curl_easy_setopt(curlHandle, CURLOPT_HTTPHEADER, headers);
    // If data is provided, set it as POSTFIELDS.
    curl_easy_setopt(curlHandle, CURLOPT_POSTFIELDS, data.c_str());
    // Set a write callback to capture response data.
    curl_easy_setopt(curlHandle, CURLOPT_WRITEFUNCTION, +[](char *ptr, size_t size, size_t nmemb, void *userdata) -> size_t
                     {
        std::string* str = static_cast<std::string*>(userdata);
        size_t totalSize = size * nmemb;
        str->append(ptr, totalSize);
        return totalSize; });
    curl_easy_setopt(curlHandle, CURLOPT_WRITEDATA, &readBuffer);

    // Perform the request.
    CURLcode res = curl_easy_perform(curlHandle);
    if (res != CURLE_OK)
    {
        response.responseCode = res;
        response.errorMessage = curl_easy_strerror(res);
    }
    else
    {
        long httpCode = 0;
        curl_easy_getinfo(curlHandle, CURLINFO_RESPONSE_CODE, &httpCode);
        response.responseCode = static_cast<int>(httpCode);
        response.content = readBuffer;
        // Optionally, try to parse JSON metadata.
        try
        {
            response.metadata = nlohmann::json::parse(readBuffer);
        }
        catch (...)
        {
            // If parsing fails, we can ignore it or set an error message.
            response.metadata = nullptr; // or handle the error as needed
        }
    }
    // Free the headers list.
    curl_slist_free_all(headers);
    return response;
}

// Create File: Upload file content to Dropbox using the "files/upload" endpoint.
DropboxResponse DropboxClient::createFile(const std::string &dropboxPath, const std::string &fileContent)
{
    std::string url = "https://content.dropboxapi.com/2/files/upload";

    // Build headers.
    struct curl_slist *headers = NULL;
    std::string authHeader = "Authorization: Bearer " + accessToken;
    headers = curl_slist_append(headers, authHeader.c_str());
    headers = curl_slist_append(headers, "Content-Type: application/octet-stream");
    std::string apiArg = "{\"path\": \"" + dropboxPath + "\", \"mode\": \"add\", \"autorename\": true, \"mute\": false}";
    std::string dropboxArgHeader = "Dropbox-API-Arg: " + apiArg;
    headers = curl_slist_append(headers, dropboxArgHeader.c_str());

    return performRequest(url, apiArg, fileContent, headers);
}

// Delete File: Deletes a file using the "files/delete_v2" endpoint.
DropboxResponse DropboxClient::deleteFile(const std::string &dropboxPath)
{
    std::string url = "https://api.dropboxapi.com/2/files/delete_v2";

    struct curl_slist *headers = NULL;
    std::string authHeader = "Authorization: Bearer " + accessToken;
    headers = curl_slist_append(headers, authHeader.c_str());
    headers = curl_slist_append(headers, "Content-Type: application/json");

    // Create JSON request data.
    nlohmann::json j;
    j["path"] = dropboxPath;
    std::string data = j.dump();

    return performRequest(url, "", data, headers);
}

DropboxResponse DropboxClient::listContent(const std::string &dropboxPath)
{
    std::string url = "https://api.dropboxapi.com/2/files/list_folder";

    // Set up headers
    struct curl_slist *headers = NULL;
    std::string authHeader = "Authorization: Bearer " + accessToken;
    headers = curl_slist_append(headers, authHeader.c_str());
    headers = curl_slist_append(headers, "Content-Type: application/json");

    // Prepare the initial JSON request
    nlohmann::json j;
    j["path"] = dropboxPath;
    j["recursive"] = true; // Set to true to list all files in subdirectories.
    std::string data = j.dump();

    // Perform the initial request
    DropboxResponse response = performRequest(url, "", data, headers);
    if (response.responseCode != 200)
    {
        // Return early if there is an error
        return response;
    }

    // Parse the initial response and collect entries
    nlohmann::json jsonResponse = nlohmann::json::parse(response.content);
    nlohmann::json allEntries = jsonResponse["entries"];

    // Continue retrieving more entries if there are additional pages
    while (jsonResponse.value("has_more", false))
    {
        // Extract the cursor for the next batch
        std::string cursor = jsonResponse["cursor"];
        std::string continueUrl = "https://api.dropboxapi.com/2/files/list_folder/continue";

        // Create JSON for the continue request
        nlohmann::json continueJson;
        continueJson["cursor"] = cursor;
        std::string continueData = continueJson.dump();

        // Perform the continue request
        DropboxResponse continueResponse = performRequest(continueUrl, "", continueData, headers);
        if (continueResponse.responseCode != 200)
        {
            // Break out if an error occurs
            break;
        }
        jsonResponse = nlohmann::json::parse(continueResponse.content);
        for (const auto &entry : jsonResponse["entries"])
        {
            allEntries.push_back(entry);
        }
    }

    // Build final aggregated JSON response
    nlohmann::json finalJson;
    finalJson["entries"] = allEntries;

    // IMPORTANT: If the last response contained a cursor, include it in the final JSON.
    if (jsonResponse.contains("cursor"))
    {
        finalJson["cursor"] = jsonResponse["cursor"];
    }

    // Update the original response with the aggregated results
    response.content = finalJson.dump();
    response.metadata = finalJson;

    return response;
}

// Modify File: Overwrite file content using the "files/upload" endpoint with mode "update".
DropboxResponse DropboxClient::modifyFile(const std::string &dropboxPath, const std::string &newContent, const std::string &rev)
{
    std::string url = "https://content.dropboxapi.com/2/files/upload";

    // Set up headers.
    struct curl_slist *headers = NULL;
    std::string authHeader = "Authorization: Bearer " + accessToken;
    headers = curl_slist_append(headers, authHeader.c_str());
    headers = curl_slist_append(headers, "Content-Type: application/octet-stream");

    // Construct the API argument JSON with mode "update" that includes the revision.
    nlohmann::json apiArgJson;
    apiArgJson["path"] = dropboxPath;
    apiArgJson["mode"] = {{".tag", "update"}, {"update", rev}};
    apiArgJson["autorename"] = false;
    apiArgJson["mute"] = false;
    std::string apiArg = apiArgJson.dump();

    std::string dropboxArgHeader = "Dropbox-API-Arg: " + apiArg;
    headers = curl_slist_append(headers, dropboxArgHeader.c_str());

    return performRequest(url, apiArg, newContent, headers);
}

// Modify Directory: Example using the "files/move_v2" endpoint to move/rename a directory.
// The JSON parameter 'params' should contain at least "from_path" and "to_path".
DropboxResponse DropboxClient::modifyDirectory(const std::string &dropboxPath, const nlohmann::json &params)
{
    std::string url = "https://api.dropboxapi.com/2/files/move_v2";

    struct curl_slist *headers = NULL;
    std::string authHeader = "Authorization: Bearer " + accessToken;
    headers = curl_slist_append(headers, authHeader.c_str());
    headers = curl_slist_append(headers, "Content-Type: application/json");

    // Here, 'params' is assumed to be correctly formed.
    std::string data = params.dump();

    return performRequest(url, "", data, headers);
}

DropboxResponse DropboxClient::readFile(const std::string &dropboxPath)
{
    std::string url = "https://content.dropboxapi.com/2/files/download";

    struct curl_slist *headers = NULL;
    std::string authHeader = "Authorization: Bearer " + accessToken;
    headers = curl_slist_append(headers, authHeader.c_str());
    // Set the proper content type for the download endpoint.
    headers = curl_slist_append(headers, "Content-Type: application/octet-stream");
    std::string apiArg = "{\"path\": \"" + dropboxPath + "\"}";
    std::string dropboxArgHeader = "Dropbox-API-Arg: " + apiArg;
    headers = curl_slist_append(headers, dropboxArgHeader.c_str());

    return performRequest(url, apiArg, "", headers);
}

DropboxResponse DropboxClient::createFolder(const std::string &dropboxPath)
{
    // Dropbox API endpoint for creating a folder.
    std::string url = "https://api.dropboxapi.com/2/files/create_folder_v2";

    // Set up headers.
    struct curl_slist *headers = NULL;
    std::string authHeader = "Authorization: Bearer " + accessToken;
    headers = curl_slist_append(headers, authHeader.c_str());
    headers = curl_slist_append(headers, "Content-Type: application/json");

    // Prepare JSON data with required parameters.
    nlohmann::json j;
    j["path"] = dropboxPath;
    j["autorename"] = false; // Set to true if you want Dropbox to auto-rename on conflict.
    std::string data = j.dump();

    // Perform the request.
    return performRequest(url, "", data, headers);
}

DropboxResponse DropboxClient::deleteFolder(const std::string &dropboxPath)
{
    // Dropbox API endpoint for deleting a file or folder.
    std::string url = "https://api.dropboxapi.com/2/files/delete_v2";

    // Set up headers.
    struct curl_slist *headers = NULL;
    std::string authHeader = "Authorization: Bearer " + accessToken;
    headers = curl_slist_append(headers, authHeader.c_str());
    headers = curl_slist_append(headers, "Content-Type: application/json");

    // Prepare JSON data specifying the folder path to delete.
    nlohmann::json j;
    j["path"] = dropboxPath;
    std::string data = j.dump();

    // Perform the request.
    return performRequest(url, "", data, headers);
}

DropboxResponse DropboxClient::getMetadata(const std::string &dropboxPath)
{
    std::string url = "https://api.dropboxapi.com/2/files/get_metadata";
    struct curl_slist *headers = NULL;
    std::string authHeader = "Authorization: Bearer " + accessToken;
    headers = curl_slist_append(headers, authHeader.c_str());
    headers = curl_slist_append(headers, "Content-Type: application/json");

    // Prepare the JSON request with the required parameters.
    nlohmann::json j;
    j["path"] = dropboxPath;
    j["include_media_info"] = false;
    j["include_deleted"] = false;
    j["include_has_explicit_shared_members"] = false;
    std::string data = j.dump();

    DropboxResponse response = performRequest(url, "", data, headers);

    // Attempt to parse the JSON content into metadata.
    try
    {
        response.metadata = nlohmann::json::parse(response.content);
    }
    catch (const std::exception &e)
    {
        response.errorMessage = e.what();
    }

    return response;
}

DropboxResponse DropboxClient::longpollFolder(const std::string &cursor, int timeout)
{
    // Correct URL for Dropbox longpoll endpoint.
    std::string url = "https://notify.dropboxapi.com/2/files/list_folder/longpoll";

    // Set up headers. Note: Authorization header is not required for longpoll.
    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, "Content-Type: application/json");

    // Prepare JSON request with the cursor and timeout.
    nlohmann::json j;
    j["cursor"] = cursor;
    j["timeout"] = timeout;
    std::string data = j.dump();

    // Perform the request.
    DropboxResponse response = performRequest(url, "", data, headers);

    // Parse the JSON response into metadata for debugging.
    try {
        response.metadata = nlohmann::json::parse(response.content);
    } catch (const std::exception &e) {
        response.errorMessage = e.what();
    }

    return response;
}


DropboxResponse DropboxClient::continueListing(const std::string &cursor)
{
    std::string continueUrl = "https://api.dropboxapi.com/2/files/list_folder/continue";

    // Set up headers.
    struct curl_slist *headers = NULL;
    std::string authHeader = "Authorization: Bearer " + accessToken;
    headers = curl_slist_append(headers, authHeader.c_str());
    headers = curl_slist_append(headers, "Content-Type: application/json");

    // Prepare JSON request with the cursor.
    nlohmann::json j;
    j["cursor"] = cursor;
    std::string data = j.dump();

    // Perform the request.
    DropboxResponse response = performRequest(continueUrl, "", data, headers);

    // Attempt to parse the response content into metadata.
    try
    {
        response.metadata = nlohmann::json::parse(response.content);
    }
    catch (const std::exception &e)
    {
        response.errorMessage = e.what();
    }

    return response;
}
