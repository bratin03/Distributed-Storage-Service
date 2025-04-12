#pragma once

#include <string>
#include <vector>
#include <random>
#include <algorithm>
#include <fstream>
#include <sstream>
#include <chrono>
#include <stdexcept>
#include <curl/curl.h>
#include "nlohmann/json.hpp"
#include "../logger/Mylogger.hpp"

// Special value used for deletion since the server only supports get and put.
#define DELETE_VALUE "__DELETE__"

namespace distributed_KV
{

    // Struct for response.
    struct Response
    {
        std::string value; // Contains the response payload if applicable.
        std::string err;   // Contains error message if any.
        bool success;
    };

    // Write callback declaration.
    size_t WriteCallback(void *contents, size_t size, size_t nmemb, void *userp);

    // Function declarations for key-value operations.
    Response sendRequest(const std::string &url, const std::string &reqType, const nlohmann::json &message);
    Response redirectToLeader(const std::string &initialUrl, const std::string &reqType, const nlohmann::json &message);
    Response tryServers(const std::vector<std::string> &servers, const std::string &reqType, const nlohmann::json &message);
    Response get(const std::vector<std::string> &servers, const std::string &key, const std::string &token);
    Response set(const std::vector<std::string> &servers, const std::string &key, const std::string &token, const std::string &value);
    Response del(const std::vector<std::string> &servers, const std::string &key, const std::string &token);

    // A simple response structure for file-based operations.
    struct FileKVResponse
    {
        bool success;
        std::string value; // For get, this will contain the JSON-dumped file info.
        std::string err;
    };

    // Inline helper: Read the entire content of a file.
    inline std::string readFileContent(const std::string &filePath)
    {
        std::ifstream file(filePath);
        if (!file)
        {
            throw std::runtime_error("Failed to open file: " + filePath);
        }
        std::stringstream buffer;
        buffer << file.rdbuf();
        return buffer.str();
    }

    // Declarations for file-based key-value operations.
    FileKVResponse setFile(const std::vector<std::string> &servers,
                           const std::string &filePath,
                           const std::string &file_content,
                           const std::string &version,
                           const std::string &token);
    FileKVResponse getFile(const std::vector<std::string> &servers,
                           const std::string &filePath,
                           const std::string &token);
    FileKVResponse deleteFile(const std::vector<std::string> &servers,
                              const std::string &filePath,
                              const std::string &token);
}
