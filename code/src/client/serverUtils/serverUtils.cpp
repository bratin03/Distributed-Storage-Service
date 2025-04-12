#include "serverUtils.hpp"

namespace serverUtils
{
    std::shared_ptr<cache::Cache> cache_instance = nullptr;
    std::string notificationLoadBalancerip;
    unsigned short notificationLoadBalancerPort;

    void initializeCache(std::chrono::milliseconds defaultTTL, std::size_t maxSize)
    {
        cache_instance = std::make_shared<cache::Cache>(defaultTTL, maxSize);
        MyLogger::info("Cache initialized with default TTL: " + std::to_string(defaultTTL.count()) + " ms and max size: " + std::to_string(maxSize) + " bytes");
    }

    json createFile(const std::string &file_key)
    {
        auto resp = login::makeRequest(login::metaLoadBalancerip, login::metaLoadBalancerPort, "/create-file", json{{"path", file_key}});
        if (resp == nullptr)
        {
            MyLogger::error("Failed to create file: " + file_key);
        }
        else
        {
            MyLogger::debug("Response from create file: " + resp.dump(4));
        }
        return resp;
    }

    json createDir(const std::string &dir_key)
    {
        auto resp = login::makeRequest(login::metaLoadBalancerip, login::metaLoadBalancerPort, "/create-directory", json{{"path", dir_key}});
        if (resp == nullptr)
        {
            MyLogger::error("Failed to create directory: " + dir_key);
        }
        else
        {
            MyLogger::debug("Response from create directory: " + resp.dump(4));
        }
        return resp;
    }

    bool uploadFile(const std::string &file_key)
    {

        metadata::File_Metadata fileMetadata(file_key);
        if (!fileMetadata.loadFromDatabase())
        {
            MyLogger::error("Failed to load file metadata from database for: " + file_key);
            return false;
        }
        auto endpoints = getFileEndpoints(file_key);
        if (endpoints.empty())
        {
            MyLogger::error("No endpoints available for file: " + file_key);
            return false;
        }
        auto content = fsUtils::readTextFile(file_key);
        auto version = fileMetadata.version;
        if (login::token.empty())
        {
            login::login();
            if (login::token.empty())
            {
                MyLogger::error("Failed to obtain login token.");
                return false;
            }
        }
        auto response = distributed_KV::setFile(endpoints, file_key, content, version, login::token);
        if (response.success)
        {
            MyLogger::info("File uploaded successfully to: " + file_key);
            // Update the metadata in the database
            fileMetadata.file_content = content;
            // Version is incremented by 1 (it is string)
            fileMetadata.version = std::to_string(std::stoi(fileMetadata.version) + 1);
            fileMetadata.content_hash = fsUtils::computeSHA256Hash(content);
            fileMetadata.fileSize = content.size();
            if (!fileMetadata.storeToDatabase())
            {
                MyLogger::error("Failed to save file metadata to database for: " + file_key);
                return false;
            }
            MyLogger::info("File metadata updated in database for: " + file_key);
            return true;
        }
        else
        {
            MyLogger::error("Failed to upload file: " + response.err);
            return false;
        }
    }

    std::vector<std::string> getFileEndpoints(const std::string &file_key)
    {
        // Check in cache first
        auto cachedEndpoints = cache_instance->get(file_key);
        if (!cachedEndpoints.empty())
        {
            MyLogger::info("Cache hit for file endpoints: " + file_key);
            return cachedEndpoints;
        }
        auto response = login::makeRequest(login::metaLoadBalancerip, login::metaLoadBalancerPort, "/get-file-endpoints", json{{"path", file_key}});
        // Check if the JSON object contains the key "endpoints" and if it's an array.
        if (response == nullptr)
        {
            MyLogger::error("Failed to get file endpoints for: " + file_key);
            return {};
        }
        else
        {
            MyLogger::debug("Response from get file endpoints: " + response.dump(4));
        }
        // Create a vector to store the endpoints.
        std::vector<std::string> endpoints;
        if (response.contains("endpoints") && response["endpoints"].is_array())
        {
            // Iterate over the array and extract each endpoint as a string.
            for (const auto &endpoint : response["endpoints"])
            {
                endpoints.push_back(endpoint.get<std::string>());
            }
        }

        // Check if the endpoints vector is empty.
        if (endpoints.empty())
        {
            MyLogger::error("No endpoints found for file: " + file_key);
            return {};
        }

        // Store the endpoints in the cache with a default TTL.
        cache_instance->set(file_key, endpoints);

        return endpoints;
    }

    bool fetchNewFile(const std::string &file_key)
    {
        MyLogger::info("Fetching new file: " + file_key);
        auto endpoints = getFileEndpoints(file_key);
        if (endpoints.empty())
        {
            MyLogger::error("No endpoints available for file: " + file_key);
            return false;
        }
        if(login::token.empty())
        {
            login::login();
            if (login::token.empty())
            {
                MyLogger::error("Failed to obtain login token.");
                return false;
            }
        }
        auto response = distributed_KV::getFile(endpoints, file_key, login::token);
        if (response.success)
        {
            json response_json = json::parse(response.value);
            MyLogger::debug("Response from fetch file: " + response_json.dump(4));
        }
        else
        {
            MyLogger::error("Failed to fetch file: " + response.err);
            return false;
        }
        return false;
    }

} // namespace serverUtils