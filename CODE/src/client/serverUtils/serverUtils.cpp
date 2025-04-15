#include "serverUtils.hpp"

namespace serverUtils
{
    std::shared_ptr<cache::Cache> cache_instance = nullptr;
    std::string notificationLoadBalancerip;
    unsigned short notificationLoadBalancerPort;
    std::string device_id;

    std::string generateConflictFilename(const std::string &path, const std::string &device)
    {
        size_t dotPos = path.rfind(".txt");
        if (dotPos == std::string::npos)
        {
            return path + "$" + device + "_conflict$"; // fallback if .txt not found
        }

        return path.substr(0, dotPos) + "$" + device + "_conflict$" + ".txt";
    }

    void initializeCache(std::chrono::milliseconds defaultTTL, std::size_t maxSize)
    {
        cache_instance = std::make_shared<cache::Cache>(defaultTTL, maxSize);
        MyLogger::info("Cache initialized with default TTL: " + std::to_string(defaultTTL.count()) + " ms and max size: " + std::to_string(maxSize) + " bytes");
    }

    json createFile(const std::string &file_key)
    {
        json payload = {{"path", file_key}};
        auto resp = login::makeRequest(login::metaLoadBalancerip, login::metaLoadBalancerPort, "/create-file", payload);
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
        auto payload = json{{"path", dir_key}};
        auto resp = login::makeRequest(login::metaLoadBalancerip, login::metaLoadBalancerPort, "/create-directory", payload);
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
        auto response = distributed_KV::setFile(endpoints, file_key, content, version, login::token,serverUtils::device_id);
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
            Conflict(file_key);
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
        auto payload = json{{"path", file_key}};
        auto response = login::makeRequest(login::metaLoadBalancerip, login::metaLoadBalancerPort, "/get-file-endpoints", payload);
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
        if (login::token.empty())
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
            // data and version_number are the keys in the JSON object
            if (response_json.contains("data") && response_json.contains("version_number"))
            {
                std::string file_content = response_json["data"];
                std::string version_number = response_json["version_number"];
                // Save the file content to the local file system
                fsUtils::createTextFile(file_key, file_content);
                MyLogger::info("File fetched successfully: " + file_key);
                // Update the metadata in the database
                metadata::File_Metadata fileMetadata(file_key);
                fileMetadata.file_content = file_content;
                fileMetadata.version = version_number;
                fileMetadata.content_hash = fsUtils::computeSHA256Hash(file_content);
                fileMetadata.fileSize = file_content.size();
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
                MyLogger::error("Invalid response format for file: " + file_key);
            }
        }
        else
        {
            MyLogger::error("Failed to fetch file: " + response.err);
            return false;
        }
        return false;
    }

    void Conflict(const std::string &file_key)
    {
        MyLogger::info("Conflict detected for file: " + file_key);
        auto local_file = fsUtils::readTextFile(file_key);
        auto endpoints = getFileEndpoints(file_key);
        if (endpoints.empty())
        {
            MyLogger::error("No endpoints available for file: " + file_key);
            return;
        }
        if (login::token.empty())
        {
            login::login();
            if (login::token.empty())
            {
                MyLogger::error("Failed to obtain login token.");
                return;
            }
        }
        auto response = distributed_KV::getFile(endpoints, file_key, login::token);
        if (response.success)
        {
            json response_json = json::parse(response.value);
            // data and version_number are the keys in the JSON object
            if (response_json.contains("data") && response_json.contains("version_number"))
            {
                std::string remote_file_content = response_json["data"];
                std::string remote_version_number = response_json["version_number"];
                // Compare the local and remote file content
                if (local_file != remote_file_content)
                {
                    MyLogger::info("Conflict detected between local and remote file: " + file_key);
                    std::string merged_content;
                    metadata::File_Metadata fileMetadata(file_key);
                    if (!fileMetadata.loadFromDatabase())
                    {
                        MyLogger::error("Failed to load file metadata from database for: " + file_key);
                    }
                    auto merge_success = merge::mergeCheck(fileMetadata.file_content, local_file, remote_file_content, merged_content);
                    if (merge_success)
                    {
                        MyLogger::info("Merge successful for file: " + file_key);
                        fsUtils::createTextFile(file_key, merged_content);
                        fileMetadata.file_content = merged_content;
                        fileMetadata.version = remote_version_number;
                        fileMetadata.content_hash = fsUtils::computeSHA256Hash(merged_content);
                        fileMetadata.fileSize = merged_content.size();
                        uploadFile(file_key);
                    }
                    else
                    {
                        MyLogger::warning("Merge not automergeable for file: " + file_key);
                        fsUtils::createTextFile(file_key, remote_file_content);
                        MyLogger::info("Local file replaced with remote version: " + file_key);
                        auto conflict_filename = generateConflictFilename(file_key, device_id);
                        fsUtils::createTextFile(conflict_filename, local_file);
                        MyLogger::info("Conflict file created: " + conflict_filename);
                        // Update the metadata in the database
                        fileMetadata.file_content = remote_file_content;
                        fileMetadata.version = remote_version_number;
                        fileMetadata.content_hash = fsUtils::computeSHA256Hash(remote_file_content);
                        fileMetadata.fileSize = remote_file_content.size();
                        if (!fileMetadata.storeToDatabase())
                        {
                            MyLogger::error("Failed to save file metadata to database for: " + file_key);
                        }
                        MyLogger::info("File metadata updated in database for: " + file_key);
                        // Send the conflicted copy to the server
                        metadata::File_Metadata conflictMetadata(conflict_filename);
                        conflictMetadata.file_content = local_file;
                        conflictMetadata.version = "0";
                        conflictMetadata.content_hash = fsUtils::computeSHA256Hash(local_file);
                        conflictMetadata.fileSize = local_file.size();
                        metadata::addFileToDirectory(conflict_filename);
                        if (!conflictMetadata.storeToDatabase())
                        {
                            MyLogger::error("Failed to save conflict file metadata to database for: " + conflict_filename);
                        }
                        MyLogger::info("Conflict file metadata updated in database for: " + conflict_filename);
                        createFile(conflict_filename);
                        uploadFile(conflict_filename);
                        try
                        {
                            AppNotify::send_notification("DSS: Conflict Detected", "A conflict was detected for the file: " + file_key + ". A conflicted copy has been created at: " + conflict_filename);
                        }
                        catch (const std::exception &e)
                        {
                            MyLogger::error("Failed to send notification: " + std::string(e.what()));
                        }
                    }
                }
                else
                {
                    MyLogger::info("No conflict detected for file: " + file_key);
                    // Update the local file with the remote version
                    metadata::File_Metadata fileMetadata(file_key);
                    fileMetadata.file_content = remote_file_content;
                    fileMetadata.version = remote_version_number;
                    fileMetadata.content_hash = fsUtils::computeSHA256Hash(remote_file_content);
                    fileMetadata.fileSize = remote_file_content.size();
                    if (!fileMetadata.storeToDatabase())
                    {
                        MyLogger::error("Failed to save file metadata to database for: " + file_key);
                    }
                    MyLogger::info("File metadata updated in database for: " + file_key);
                }
            }
            else
            {
                MyLogger::error("Invalid response format for file: " + file_key);
            }
        }
        else
        {
            MyLogger::error("Failed to fetch file: " + response.err);
        }
    }

    bool deleteFile(const std::string &file_key)
    {
        auto payload = json{{"path", file_key}};
        auto resp = login::makeRequest(login::metaLoadBalancerip, login::metaLoadBalancerPort, "/delete", payload);
        if (resp == nullptr)
        {
            MyLogger::error("Failed to delete file: " + file_key);
            return false;
        }
        else
        {
            MyLogger::debug("Response from delete file: " + resp.dump(4));
        }
        return true;
    }

} // namespace serverUtils