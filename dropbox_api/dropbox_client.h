#ifndef DROPBOX_CLIENT_H
#define DROPBOX_CLIENT_H

#include <string>
#include <nlohmann/json.hpp>
#include <curl/curl.h>

// Struct to hold a Dropbox response.
struct DropboxResponse
{
    int responseCode;         // HTTP response code or CURL error code.
    std::string errorMessage; // Error message, if any.
    std::string content;      // Content of the response (e.g. file content or API JSON result).
    nlohmann::json metadata;  // Parsed JSON metadata (if applicable).
};

class DropboxClient
{
public:
    // Constructor that takes the path to a config file and reads the access token.
    DropboxClient(const std::string &configPath);
    ~DropboxClient();

    // Dropbox operations.
    DropboxResponse createFile(const std::string &dropboxPath, const std::string &fileContent);
    DropboxResponse deleteFile(const std::string &dropboxPath);
    DropboxResponse listContent(const std::string &dropboxPath);
    DropboxResponse modifyFile(const std::string &dropboxPath, const std::string &newContent);
    // For modifyDirectory, we pass a JSON object for parameters (e.g. moving or renaming).
    DropboxResponse modifyDirectory(const std::string &dropboxPath, const nlohmann::json &params);
    DropboxResponse readFile(const std::string &dropboxPath);

private:
    std::string accessToken;
    CURL *curlHandle;

    // Helper function to perform an HTTP request.
    DropboxResponse performRequest(const std::string &url,
                                   const std::string &args,
                                   const std::string &data,
                                   struct curl_slist *headers);
};

#endif // DROPBOX_CLIENT_H
