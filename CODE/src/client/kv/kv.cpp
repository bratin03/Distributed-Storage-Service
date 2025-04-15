#include "kv.hpp"
#include <cstdio>

namespace distributed_KV
{

    size_t WriteCallback(void *contents, size_t size, size_t nmemb, void *userp)
    {
        std::string *s = static_cast<std::string *>(userp);
        size_t totalSize = size * nmemb;
        s->append(static_cast<char *>(contents), totalSize);
        return totalSize;
    }

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

    Response get(const std::vector<std::string> &servers, const std::string &key, const std::string &token)
    {
        printf("Getting key: %s\n", key.c_str());
        nlohmann::json payload = {{"key", key}, {"token", token}};
        nlohmann::json message = {{"type", "get"}, {"payload", payload}};
        Response res = tryServers(servers, "get", message);
        if (res.success)
        {
            try
            {
                auto jsonResponse = nlohmann::json::parse(res.value);
                MyLogger::debug("Response from server: " + jsonResponse.dump(4));
                // Check if the server indicates a failure (e.g., key never set)
                if (jsonResponse.contains("code") && jsonResponse["code"] == "fail")
                {
                    return {"", "Error: Key does not exist", false};
                }
                // Check if payload contains a value
                if (jsonResponse.contains("payload"))
                {
                    auto payload = jsonResponse["payload"];
                    // If payload is a string, serialize it.
                    if (payload.is_string())
                    {
                        payload = nlohmann::json::parse(payload.get<std::string>());
                    }
                    if (payload.contains("data"))
                    {
                        // Check if it is deleted
                        if (payload["data"] == DELETE_VALUE)
                        {
                            return {"", "Error: Key does not exist", false};
                        }
                        MyLogger::debug("Value from server: " + payload.dump(4));
                        return {payload.dump(), "Success", true};
                    }
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

    Response set(const std::vector<std::string> &servers, const std::string &key, const std::string &token,const std::string &device_id, const std::string &value)
    {
        printf("Setting key: %s with value: %s\n", key.c_str(), value.c_str());
        nlohmann::json payload = {{"key", key}, {"token", token},{"device_id",device_id}, {"value", value}};
        nlohmann::json message = {{"type", "put"}, {"payload", payload}};
        return tryServers(servers, "put", message);
    }

    Response del(const std::vector<std::string> &servers, const std::string &key, const std::string &token,const std::string &device_id)
    {
        return set(servers, key, token,device_id, DELETE_VALUE);
    }

    FileKVResponse setFile(const std::vector<std::string> &servers,
                           const std::string &filePath,
                           const std::string &file_content,
                           const std::string &version,
                           const std::string &token,
                           const std::string &device_id)
    {
        FileKVResponse response;
        try
        {
            nlohmann::json j;
            j["version_number"] = version;
            j["data"] = file_content;
            // Use filePath as the key
            Response setResp = set(servers, filePath, token,device_id, j.dump());
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

    FileKVResponse getFile(const std::vector<std::string> &servers,
                           const std::string &filePath,
                           const std::string &token)
    {
        FileKVResponse response;
        Response getResp = get(servers, filePath, token);
        if (getResp.success)
        {
            try
            {
                auto j = nlohmann::json::parse(getResp.value);
                // For now, return the pretty-printed JSON.
                response.success = true;
                response.value = j.dump();
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

    FileKVResponse deleteFile(const std::vector<std::string> &servers,
                              const std::string &filePath,
                              const std::string &token,
                              const std::string &device_id)
    {
        FileKVResponse response;
        Response delResp = del(servers, filePath, token,device_id);
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

} // end namespace distributed_KV
