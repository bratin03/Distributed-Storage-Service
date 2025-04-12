#ifndef KV_CLIENT_HPP
#define KV_CLIENT_HPP

#include <string>
#include <vector>
#include <random>
#include <algorithm>
#include <fstream>
#include <chrono>
#include <curl/curl.h>
#include "nlohmann/json.hpp"

// Special value used for deletion since the server only supports get and put.
#define DELETE_VALUE "__DELETE__"

// Struct for response.
namespace distributed_KV
{

    struct Response
    {
        std::string value; // Contains the response payload if applicable.
        std::string err;   // Contains error message if any.
        bool success;
    };

    // Helper: CURL write callback to accumulate response data.
    static size_t WriteCallback(void *contents, size_t size, size_t nmemb, void *userp)
    {
        std::string *s = static_cast<std::string *>(userp);
        size_t totalSize = size * nmemb;
        s->append(static_cast<char *>(contents), totalSize);
        return totalSize;
    }

    // Helper: Send an HTTP request (GET or PUT) with a JSON payload.
    Response sendRequest(const std::string &url, const std::string &reqType, const nlohmann::json &message)
    {
        Response res{"", "", false};
        CURL *curl = curl_easy_init();
        if (!curl)
        {
            res.err = "Failed to initialize CURL";
            return res;
        }

        std::string readBuffer;
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 1L); // 1 second timeout
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);

        // Set HTTP header to indicate JSON
        struct curl_slist *headers = nullptr;
        headers = curl_slist_append(headers, "Content-Type: application/json");
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

        // Convert JSON message to string.
        std::string json_str = message.dump();

        if (reqType == "get")
        {
            // Although GET with a body is not standard, we mimic the behavior.
            curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "GET");
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json_str.c_str());
        }
        else if (reqType == "put")
        {
            curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PUT");
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json_str.c_str());
        }

        CURLcode curlRes = curl_easy_perform(curl);
        if (curlRes != CURLE_OK)
        {
            res.err = curl_easy_strerror(curlRes);
            curl_easy_cleanup(curl);
            curl_slist_free_all(headers);
            return res;
        }
        long httpCode = 0;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);
        curl_easy_cleanup(curl);
        curl_slist_free_all(headers);

        if (httpCode == 200)
        {
            res.success = true;
            res.value = readBuffer;
        }
        else
        {
            res.err = "HTTP error code: " + std::to_string(httpCode);
        }
        return res;
    }

    // Implements the redirection logic: if the server responds with a valid JSON payload
    // that includes a "message" field (interpreted as a new leader address),
    // it will update the URL and repeat the request.
    Response redirectToLeader(const std::string &initialUrl, const std::string &reqType, const nlohmann::json &message)
    {
        std::string currentUrl = initialUrl;
        Response res;
        while (true)
        {
            res = sendRequest(currentUrl, reqType, message);
            if (!res.success)
            {
                break; // error communicating with server
            }
            try
            {
                auto jsonResponse = nlohmann::json::parse(res.value);
                if (jsonResponse.contains("payload") && jsonResponse["payload"].contains("message"))
                {
                    // Redirect to the new leader indicated in "message"
                    std::string newAddress = jsonResponse["payload"]["message"];
                    currentUrl = newAddress + "/request";
                    continue;
                }
            }
            catch (std::exception &e)
            {
                // If parsing fails, assume no redirection.
                break;
            }
            break;
        }
        return res;
    }

    // Utility: Given a vector of server IP addresses, shuffle the vector and try each one until one returns a valid response.
    Response tryServers(const std::vector<std::string> &servers, const std::string &reqType, const nlohmann::json &message)
    {
        std::vector<std::string> shuffledServers = servers;
        std::random_device rd;
        std::mt19937 g(rd());
        std::shuffle(shuffledServers.begin(), shuffledServers.end(), g);

        Response finalResponse;
        for (const auto &server : shuffledServers)
        {
            std::string url = server + "/request";
            finalResponse = redirectToLeader(url, reqType, message);
            if (finalResponse.success)
            {
                return finalResponse;
            }
        }
        finalResponse.err = "All servers unreachable or returned errors.";
        return finalResponse;
    }

    /**
     * @brief Retrieves the value for a given key from the distributed key-value store.
     *
     * This function sends a GET request to one of the provided servers. It first attempts
     * to retrieve the key. If the key has never been set or has been deleted (i.e. the value
     * equals the deletion marker), it returns an error.
     *
     * @param servers A vector of server addresses (e.g., "http://127.0.0.1:5000").
     * @param key The key to be retrieved.
     * @return Response struct containing:
     *         - success: true if the key exists and was retrieved; false otherwise.
     *         - value: the retrieved value if successful.
     *         - err: an error message if the key does not exist or an error occurred.
     */
    Response get(const std::vector<std::string> &servers, const std::string &key,const std::string &token)
    {
        printf("Getting key: %s\n", key.c_str());
        nlohmann::json payload = {{"key", key},{"token",token}};
        nlohmann::json message = {{"type", "get"}, {"payload", payload}};
        Response res = tryServers(servers, "get", message);
        if (res.success)
        {
            try
            {
                auto jsonResponse = nlohmann::json::parse(res.value);
                // Check if the server indicates a failure (e.g., key never set)
                if (jsonResponse.contains("code") && jsonResponse["code"] == "fail")
                {
                    return {"", "Error: Key does not exist", false};
                }
                // Check if payload contains a value
                if (jsonResponse.contains("payload") && jsonResponse["payload"].contains("value"))
                {
                    std::string value = jsonResponse["payload"]["value"];
                    if (value == DELETE_VALUE)
                    {
                        return {"", "Error: Key does not exist", false};
                    }
                    res.value = value;
                }
                else
                {
                    // No value returned, treat as key does not exist.
                    return {"", "Error: Key does not exist", false};
                }
            }
            catch (std::exception &e)
            {
                return {"", std::string("JSON parsing error: ") + e.what(), false};
            }
        }
        return res;
    }

    /**
     * @brief Sets the value for a given key in the distributed key-value store.
     *
     * This function sends a PUT request to one of the provided servers to store a key-value pair.
     *
     * @param servers A vector of server addresses.
     * @param key The key to be set.
     * @param value The value to be stored for the key.
     * @return Response struct containing:
     *         - success: true if the value was set successfully; false otherwise.
     *         - value: the response from the server if applicable.
     *         - err: an error message if the operation failed.
     */
    Response set(const std::vector<std::string> &servers, const std::string &key,const std::string &token, const std::string &value)
    {
        printf("Setting key: %s with value: %s\n", key.c_str(), value.c_str());
        nlohmann::json payload = {{"key", key},{"token",token}, {"value", value}};
        nlohmann::json message = {{"type", "put"}, {"payload", payload}};
        return tryServers(servers, "put", message);
    }

    /**
     * @brief Deletes a key from the distributed key-value store.
     *
     * Since the server only supports get and put, deletion is implemented by setting the key's
     * value to a special deletion marker.
     *
     * @param servers A vector of server addresses.
     * @param key The key to be deleted.
     * @return Response struct containing:
     *         - success: true if the deletion was processed successfully; false otherwise.
     *         - value: the response from the server if applicable.
     *         - err: an error message if the operation failed.
     */
    Response del(const std::vector<std::string> &servers, const std::string &key,const std::string &token)
    {
        return set(servers, key,token, DELETE_VALUE);
    }

    // A simple response structure for file-based operations.
    struct FileKVResponse
    {
        bool success;
        std::string value; // For get, this will contain the JSON-dumped file info.
        std::string err;
    };

    // Helper function to read the entire content of a file.
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

    // Sets the key (file path) with a JSON value containing the version and file content.
    inline FileKVResponse setFile(const std::vector<std::string> &servers,
                                  const std::string &filePath,
                                  const std::string &file_content,
                                  const std::string &version,
                                  const std::string &token)
    {
        FileKVResponse response;
        try
        {
            // Read file content
            nlohmann::json j;
            j["version_number"] = version;
            j["data"] = file_content;
            // Use filePath as the key
            Response setResp = set(servers, filePath,token, j.dump());
            response.success = setResp.success;
            if (setResp.success)
            {
                response.value = setResp.value;
            }
            else
            {
                response.err = setResp.err;
            }
        }
        catch (const std::exception &e)
        {
            response.success = false;
            response.err = e.what();
        }
        return response;
    }

    // Gets the stored JSON value for a given file key. It returns the version and file data.
    inline FileKVResponse getFile(const std::vector<std::string> &servers,
                                  const std::string &filePath,
                                  const std::string &token

    )
    {
        FileKVResponse response;
        Response getResp = get(servers,filePath,token);
        if (getResp.success)
        {
            try
            {
                nlohmann::json j = nlohmann::json::parse(getResp.value);
                // Here you could extract specific fields if needed:
                // std::string version = j["version_number"];
                // std::string data = j["data"];
                // For now, we return the pretty-printed JSON.
                response.success = true;
                response.value = j;
            }
            catch (const std::exception &e)
            {
                response.success = false;
                response.err = e.what();
            }
        }
        else
        {
            response.success = false;
            response.err = getResp.err;
        }
        return response;
    }

    // Deletes the key (file path) from the distributed key–value store.
    inline FileKVResponse deleteFile(const std::vector<std::string> &servers,
                                     const std::string &filePath,
                                     const std::string &token)
    {
        FileKVResponse response;

        Response delResp = del(servers, filePath,token);
        response.success = delResp.success;
        if (delResp.success)
        {
            response.value = delResp.value;
        }
        else
        {
            response.err = delResp.err;
        }
        return response;
    }

}

#endif // KV_CLIENT_HPP
