#include "../logger/logger.h"
#include "dropbox_client.h"
#include <filesystem>
#include <fstream>
#include <memory>
#include <nlohmann/json.hpp>
#include <rocksdb/db.h>
#include <string>
#include <vector>
#include "../hash/dropbox_content_hash.hpp"

// A utility class for filesystem operations.
class FileSystemManager
{
public:
  static void createDirectory(const std::string &path, const std::string &basePath)
  {
    std::filesystem::path dirPath = std::filesystem::path(basePath) / path;
    std::error_code ec;
    bool created = std::filesystem::create_directories(dirPath, ec);
    if (ec)
    {
      Logger::error("Error creating directory '" + dirPath.string() +
                    "': " + ec.message());
    }
    else if (created)
    {
      Logger::info("Created directory: " + dirPath.string());
    }
    else
    {
      Logger::info("Directory already exists: " + dirPath.string());
    }
  }

  static void createFile(const std::string &path, const std::string &basePath, const std::string &content)
  {
    std::filesystem::path filePath = std::filesystem::path(basePath) / path;
    std::ofstream file(filePath);
    if (file.is_open())
    {
      file << content;
      file.close();
      Logger::info("Created file: " + filePath.string());
    }
    else
    {
      Logger::error("Error creating file '" + filePath.string() + "'");
    }
  }

  static std::string readFileContent(const std::string &path, const std::string &basePath)
  {
    std::filesystem::path filePath = std::filesystem::path(basePath) / path;
    std::ifstream file(filePath);
    if (file.is_open())
    {
      std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
      file.close();
      return content;
    }
    else
    {
      Logger::error("Error reading file '" + filePath.string() + "'");
      return "";
    }
  }
};

// Helper function to ensure the Dropbox folder for the user exists.
void ensureDropboxUserFolder(std::shared_ptr<DropboxClient> dropboxClient, const std::string &user)
{
  auto folderMetadata = dropboxClient->getMetadata(user);
  Logger::info("Folder metadata response code: " + std::to_string(folderMetadata.responseCode));
  Logger::info("Folder metadata content: " + folderMetadata.content);
  if (folderMetadata.responseCode == 409)
  {
    auto createFolderResponse = dropboxClient->createFolder(user);
    if (createFolderResponse.responseCode != 200)
    {
      Logger::error("Failed to create folder in Dropbox: " + createFolderResponse.content);
    }
  }
}

// Helper function to retrieve and parse the Dropbox folder content.
nlohmann::json getDropboxContent(std::shared_ptr<DropboxClient> dropboxClient, const std::string &user)
{
  auto response = dropboxClient->listContent(user + "/");
  if (response.responseCode != 200)
  {
    Logger::error("Failed to list content in Dropbox: " + response.content);
    return nlohmann::json();
  }
  return nlohmann::json::parse(response.content);
}

// A class responsible for processing Dropbox JSON entries.
class EntryProcessor
{
public:
  EntryProcessor(std::shared_ptr<rocksdb::DB> db, std::shared_ptr<DropboxClient> dropboxClient, const std::string &user, const std::string &monitoringDirectory) : db_(db), dropboxClient_(dropboxClient), user_(user), monitoringDirectory_(monitoringDirectory) {}

  // Process all entries: first directories, then files.
  void processEntries(const nlohmann::json &entries)
  {
    // Process directories first.
    for (const auto &entry : entries)
    {
      if (entry[".tag"] == "folder")
      {
        processDirectory(entry);
      }
    }
    // Then process files.
    for (const auto &entry : entries)
    {
      if (entry[".tag"] == "file")
      {
        processFile(entry);
      }
    }
  }

private:
  // Process a directory entry.
  void processDirectory(const nlohmann::json &entry)
  {
    std::string name = entry["path_display"];
    name += "/";
    Logger::info("Found folder: " + name);
    Directory_Metadata metadata;
    metadata.setDirectoryName(name);
    if (!metadata.loadFromDatabase(db_))
    {
      Logger::info("Directory not found in database: " + name);
      metadata.storeToDatabase(db_);
      // Remove the user part from the full path.
      std::string relativePath = name.substr(user_.length() + 1);
      // Create the directory in the local filesystem.
      FileSystemManager::createDirectory(relativePath, monitoringDirectory_);
      Logger::info("Stored new directory metadata in database: " + relativePath);
    }
  }

  // Process a file entry.
  void processFile(const nlohmann::json &entry)
  {
    std::string name = entry["path_display"];
    Logger::info("Found file: " + name);
    File_Metadata fileMetadata;
    fileMetadata.setFileName(name);
    if (!fileMetadata.loadFromDatabase(db_))
    {
      Logger::info("File not found in database: " + fileMetadata.getFileName());
      fileMetadata.content_hash = entry["content_hash"];
      fileMetadata.fileSize = entry["size"];
      fileMetadata.rev = entry["rev"];
      fileMetadata.latest_rev = entry["rev"];

      // Download file content using a dedicated helper.
      std::string content = downloadFileContent(fileMetadata.getFileName());
      fileMetadata.file_content = content;
      // Remove the user part from the full path.
      std::string relativePath = fileMetadata.getFileName().substr(user_.length() + 1);
      FileSystemManager::createFile(relativePath, monitoringDirectory_, fileMetadata.file_content);
      fileMetadata.storeToDatabase(db_);
      updateDirectoryMetadata(fileMetadata.getFileName());
      Logger::info("Stored new file metadata in database: " + fileMetadata.getFileName());
    }
    else
    {
      // Comare the content hash with the existing one.
      // First from the path remove the user part.
      std::string relativePath = fileMetadata.getFileName().substr(user_.length() + 1);
      auto fileContent = FileSystemManager::readFileContent(relativePath, monitoringDirectory_);
      auto fileHash = dropbox::compute_content_hash(fileContent);
      Logger::info("File hash: " + fileHash);
      std::string serverFileHash = entry["content_hash"];
      if(fileHash != serverFileHash)
      {
        Logger::info("File content hash mismatch for: " + fileMetadata.getFileName());
      }
      else
      {
        fileMetadata.content_hash = entry["content_hash"];
        fileMetadata.fileSize = entry["size"];
        fileMetadata.rev = entry["rev"];
        fileMetadata.latest_rev = entry["rev"];
        fileMetadata.storeToDatabase(db_);
      }
    }
  }

  // Helper to download file content.
  std::string downloadFileContent(const std::string &filePath)
  {
    auto downloadResponse = dropboxClient_->readFile(filePath);
    Logger::info("File download response code: " + std::to_string(downloadResponse.responseCode));
    Logger::info("File download content: " + downloadResponse.content);
    if (downloadResponse.responseCode == 200)
    {
      return downloadResponse.content;
    }
    return "";
  }

  // Helper to update the directory metadata for a given file.
  void updateDirectoryMetadata(const std::string &filePath)
  {
    Directory_Metadata dirMetadata;
    std::string dirPath = filePath.substr(0, filePath.find_last_of('/')) + "/";
    Logger::info("Updating directory metadata for: " + dirPath);
    dirMetadata.setDirectoryName(dirPath);
    if (!dirMetadata.loadFromDatabase(db_))
    {
      Logger::info("Directory not found in database: " + dirPath);
      dirMetadata.storeToDatabase(db_);
    }
    dirMetadata.files.push_back(filePath);
    dirMetadata.storeToDatabase(db_);
  }

  std::shared_ptr<rocksdb::DB> db_;
  std::shared_ptr<DropboxClient> dropboxClient_;
  std::string user_;
  std::string monitoringDirectory_;
};

// The bootup_2 function remains as a free function.
void bootup_2(std::shared_ptr<rocksdb::DB> db,
              std::shared_ptr<DropboxClient> dropboxClient,
              nlohmann::json &config)
{
  if (config.contains("user"))
  {
    std::string user = config["user"];
    Logger::info("Starting synchronization for user: " + user);

    // Ensure the Dropbox folder for the user exists.
    ensureDropboxUserFolder(dropboxClient, user);

    // Retrieve Dropbox folder content.
    nlohmann::json content = getDropboxContent(dropboxClient, user);
    if (content.is_null() || !content.contains("entries"))
    {
      Logger::error("No valid entries found in Dropbox content.");
      return;
    }
    Logger::info("Content in Dropbox folder: " + content.dump());

    // Process the entries.
    EntryProcessor processor(db, dropboxClient, user, config["monitoring_directory"]);
    processor.processEntries(content["entries"]);
  }
}
