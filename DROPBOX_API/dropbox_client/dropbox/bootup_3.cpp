#include "../logger/logger.h"
#include "dropbox_client.h"
#include <memory>
#include <nlohmann/json.hpp>
#include <rocksdb/db.h>
#include <string>
#include <map>

// Helper function: Check if file has a .txt extension.
bool isTxtFile(const std::string &fileName)
{
    return (fileName.size() >= 4 && fileName.substr(fileName.size() - 4) == ".txt");
}

// Helper function: Process missing file (only .txt files)
bool processMissingFile(const std::string &fileKey,
                        std::shared_ptr<rocksdb::DB> db,
                        const std::string &user,
                        const std::string &base_path,
                        std::shared_ptr<DropboxClient> dropboxClient)
{
    // Make sure we process only .txt files.
    if (!isTxtFile(fileKey))
    {
        Logger::debug("Skipping non-txt file: " + fileKey);
        return true;
    }

    Logger::debug("Processing missing file: " + fileKey);
    Logger::info("File not found on server: " + fileKey);
    File_Metadata fileMetadata;
    fileMetadata.setFileName(fileKey);
    if (!fileMetadata.loadFromDatabase(db))
    {
        Logger::error("File metadata not found in database: " + fileKey);
        return false;
    }

    // Calculate relative path (remove the user prefix and slash)
    std::string relativePath = fileKey.substr(user.length() + 1);
    Logger::debug("Computed relative path: " + relativePath);
    std::string content = FileSystemUtil::readFileContent(relativePath, base_path);
    Logger::debug("Read file content for: " + fileKey);

    auto response = dropboxClient->createFile(fileKey, content);
    if (response.responseCode == 200)
    {
        Logger::info("File uploaded to server: " + fileKey);
        auto responseJson = nlohmann::json::parse(response.content);
        fileMetadata.content_hash = responseJson["content_hash"];
        fileMetadata.fileSize = responseJson["size"];
        fileMetadata.rev = responseJson["rev"];
        fileMetadata.latest_rev = responseJson["rev"];
        if (fileMetadata.storeToDatabase(db))
        {
            Logger::info("File metadata updated in database: " + fileKey);
        }
        else
        {
            Logger::error("Failed to update file metadata in database: " + fileKey);
        }
        return true;
    }
    else
    {
        Logger::error("Failed to upload file to server: " + fileKey +
                      " Error: " + response.content);
        return false;
    }
}

// Helper function: Process missing directory (unchanged)
bool processMissingDirectory(const std::string &dirKey,
                             std::shared_ptr<DropboxClient> dropboxClient)
{
    Logger::debug("Processing missing directory: " + dirKey);
    Logger::info("Directory not found on server: " + dirKey);
    std::string dirName = dirKey;
    // Remove trailing slash if exists.
    if (!dirName.empty() && dirName.back() == '/')
    {
        dirName.pop_back();
        Logger::debug("Removed trailing slash, directory name now: " + dirName);
    }
    auto folderResponse = dropboxClient->createFolder(dirName);
    if (folderResponse.responseCode == 200)
    {
        Logger::info("Created directory on server: " + dirKey);
        return true;
    }
    else
    {
        Logger::error("Failed to create directory on server: " + dirKey +
                      " Error: " + folderResponse.content);
        return false;
    }
}

// Recursive function to synchronize directories and files.
void syncDirectoryRecursively(const std::string &current_dir,
                              std::shared_ptr<rocksdb::DB> db,
                              std::map<std::string, bool> &fileExists,
                              const std::string &user,
                              const std::string &base_path,
                              std::shared_ptr<DropboxClient> dropboxClient)
{
    Logger::debug("Synchronizing directory: " + current_dir);
    // Load the metadata for the current directory.
    Directory_Metadata metadata;
    metadata.setDirectoryName(current_dir);
    if (!metadata.loadFromDatabase(db))
    {
        Logger::error("Directory not found in database: " + current_dir);
        return;
    }
    Logger::debug("Loaded metadata for directory: " + current_dir);

    // Process files in the current directory.
    for (const auto &fileKey : metadata.files)
    {
        // Only process .txt files.
        if (!isTxtFile(fileKey))
        {
            Logger::debug("Skipping non-txt file: " + fileKey);
            continue;
        }
        Logger::debug("Checking file: " + fileKey + " in directory: " + current_dir);
        if (fileExists.find(fileKey) == fileExists.end())
        {
            Logger::info("Missing file detected: " + fileKey);
            processMissingFile(fileKey, db, user, base_path, dropboxClient);
        }
        else
        {
            Logger::debug("File exists on server: " + fileKey);
        }
    }

    // Process subdirectories in the current directory.
    for (const auto &subDirKey : metadata.directories)
    {
        Logger::debug("Checking subdirectory: " + subDirKey + " in directory: " + current_dir);
        if (fileExists.find(subDirKey) == fileExists.end())
        {
            Logger::info("Missing directory detected: " + subDirKey);
            if (!processMissingDirectory(subDirKey, dropboxClient))
            {
                Logger::error("Stopping recursion due to directory creation failure: " + subDirKey);
                return;
            }
        }
        else
        {
            Logger::debug("Subdirectory exists on server: " + subDirKey);
        }
        // Recurse into subdirectories.
        syncDirectoryRecursively(subDirKey, db, fileExists, user, base_path, dropboxClient);
    }
}

// Exposed bootup function
void bootup_3(std::shared_ptr<rocksdb::DB> db,
              std::shared_ptr<DropboxClient> dropboxClient,
              nlohmann::json &config)
{
    Logger::info("Bootup 3 started.");
    std::string user = config["user"];
    Logger::debug("User from config: " + user);
    auto dropboxListResponse = dropboxClient->listContent(user);
    Logger::debug("Dropbox list content response: " + dropboxListResponse.content);
    nlohmann::json entries = nlohmann::json::parse(dropboxListResponse.content);
    entries = entries["entries"];
    Logger::debug("Parsed Dropbox entries.");

    std::map<std::string, bool> fileExists;
    // Mark existing files and directories from Dropbox, filtering for .txt files.
    for (const auto &entry : entries)
    {
        if (entry[".tag"] == "file")
        {
            std::string fileName = entry["path_display"];
            // Only mark .txt files.
            if (isTxtFile(fileName))
            {
                fileExists[fileName] = true;
                Logger::debug("Marked existing file: " + fileName);
            }
            else
            {
                Logger::debug("Skipping non-txt file in Dropbox listing: " + fileName);
            }
        }
        else if (entry[".tag"] == "folder")
        {
            // For directories, keep the trailing slash.
            std::string folderName = std::string(entry["path_display"]) + "/";
            fileExists[folderName] = true;
            Logger::debug("Marked existing folder: " + folderName);
        }
    }

    std::string base_path = config["monitoring_directory"];
    Logger::debug("Base path from config: " + base_path);
    std::string current_dir = user + "/";
    Logger::info("Starting synchronization for directory: " + current_dir);
    syncDirectoryRecursively(current_dir, db, fileExists, user, base_path, dropboxClient);
    Logger::info("Finished creating missing files at server.");
}
