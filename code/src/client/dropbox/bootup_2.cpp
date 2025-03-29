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
#include "../merge/three_way_merge.hpp"

namespace FileSystemUtil
{

  // Helper to build a filesystem path from base and relative parts.
  std::filesystem::path buildPath(const std::string &base, const std::string &relative)
  {
    return std::filesystem::path(base) / relative;
  }

  // Create a directory with error logging.
  void createDirectory(const std::string &relativePath, const std::string &basePath)
  {
    auto dirPath = buildPath(basePath, relativePath);
    std::error_code ec;
    bool created = std::filesystem::create_directories(dirPath, ec);
    if (ec)
    {
      Logger::error("Error creating directory '" + dirPath.string() + "': " + ec.message());
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

  // Create a file with provided content.
  void createFile(const std::string &relativePath, const std::string &basePath, const std::string &content)
  {
    auto filePath = buildPath(basePath, relativePath);
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

  // Read and return file content.
  std::string readFileContent(const std::string &relativePath, const std::string &basePath)
  {
    auto filePath = buildPath(basePath, relativePath);
    std::ifstream file(filePath);
    if (file.is_open())
    {
      std::string content((std::istreambuf_iterator<char>(file)),
                          std::istreambuf_iterator<char>());
      file.close();
      return content;
    }
    else
    {
      Logger::error("Error reading file '" + filePath.string() + "'");
      return "";
    }
  }

} // namespace FileSystemUtil

namespace DropboxAPI
{

  // Group Dropbox-related operations together.
  void ensureUserFolder(std::shared_ptr<DropboxClient> dropboxClient, const std::string &user)
  {
    auto metadata = dropboxClient->getMetadata(user);
    Logger::info("User folder metadata response code: " + std::to_string(metadata.responseCode));
    Logger::info("User folder metadata content: " + metadata.content);
    if (metadata.responseCode == 409)
    {
      auto createResponse = dropboxClient->createFolder(user);
      if (createResponse.responseCode != 200)
      {
        Logger::error("Failed to create folder on Dropbox: " + createResponse.content);
      }
    }
  }

  nlohmann::json getFolderContent(std::shared_ptr<DropboxClient> dropboxClient, const std::string &user)
  {
    auto response = dropboxClient->listContent(user + "/");
    if (response.responseCode != 200)
    {
      Logger::error("Failed to list content on Dropbox: " + response.content);
      return nlohmann::json();
    }
    return nlohmann::json::parse(response.content);
  }

} // namespace DropboxAPI

// The DropboxSyncManager handles synchronization between Dropbox and the local system.
class DropboxSyncManager
{
public:
  DropboxSyncManager(std::shared_ptr<rocksdb::DB> db,
                     std::shared_ptr<DropboxClient> dropboxClient,
                     const std::string &user,
                     const std::string &monitoringDirectory)
      : db_(db), dropboxClient_(dropboxClient), user_(user), monitoringDirectory_(monitoringDirectory) {}

  // Synchronize directories and files based on entries from Dropbox.
  void synchronize(const nlohmann::json &entries)
  {
    processEntries(entries, "folder");
    processEntries(entries, "file");
  }

private:
  // Process entries based on tag (either "folder" or "file").
  void processEntries(const nlohmann::json &entries, const std::string &tag)
  {
    for (const auto &entry : entries)
    {
      if (entry[".tag"] == tag)
      {
        (tag == "folder") ? processDirectoryEntry(entry) : processFileEntry(entry);
      }
    }
  }

  // Process a directory entry.
  void processDirectoryEntry(const nlohmann::json &entry)
  {
    std::string fullPath = entry["path_display"];
    if (fullPath.back() != '/')
      fullPath += "/";
    Logger::info("Processing directory: " + fullPath);

    Directory_Metadata metadata;
    metadata.setDirectoryName(fullPath);
    if (!metadata.loadFromDatabase(db_))
    {
      Logger::info("Directory not found in database: " + fullPath);
      metadata.storeToDatabase(db_);
      std::string relativePath = fullPath.substr(user_.length() + 1);
      FileSystemUtil::createDirectory(relativePath, monitoringDirectory_);
      Logger::info("Stored new directory metadata: " + relativePath);
    }
  }

  // Process a file entry by determining if it is new or updated.
  void processFileEntry(const nlohmann::json &entry)
  {
    std::string fullPath = entry["path_display"];
    Logger::info("Processing file: " + fullPath);

    File_Metadata fileMetadata;
    fileMetadata.setFileName(fullPath);
    std::string relativePath = fullPath.substr(user_.length() + 1);

    if (!fileMetadata.loadFromDatabase(db_))
    {
      handleNewFile(entry, fileMetadata, relativePath);
    }
    else
    {
      handleExistingFile(entry, fileMetadata, relativePath);
    }
  }

  // Handle new files (not found in the local database).
  void handleNewFile(const nlohmann::json &entry, File_Metadata &fileMetadata, const std::string &relativePath)
  {
    Logger::info("New file detected: " + fileMetadata.getFileName());
    fileMetadata.content_hash = entry["content_hash"];
    fileMetadata.fileSize = entry["size"];
    fileMetadata.rev = entry["rev"];
    fileMetadata.latest_rev = entry["rev"];

    std::string content = downloadFileContent(fileMetadata.getFileName());
    fileMetadata.file_content = content;
    FileSystemUtil::createFile(relativePath, monitoringDirectory_, content);
    fileMetadata.storeToDatabase(db_);
    updateDirectoryMetadata(fileMetadata.getFileName());
    Logger::info("Stored new file metadata for: " + fileMetadata.getFileName());
  }

  // Handle existing files by checking for changes and possible merge conflicts.
  void handleExistingFile(const nlohmann::json &entry, File_Metadata &fileMetadata, const std::string &relativePath)
  {
    Logger::info("Existing file found: " + fileMetadata.getFileName());
    auto localContent = FileSystemUtil::readFileContent(relativePath, monitoringDirectory_);
    auto localHash = dropbox::compute_content_hash(localContent);
    Logger::info("Local hash: " + localHash);
    std::string serverHash = entry["content_hash"];

    if (localHash != serverHash)
    {
      Logger::info("Hash mismatch detected for file: " + fileMetadata.getFileName());
      resolveFileConflict(entry, fileMetadata, localContent, localHash, relativePath);
    }
    else
    {
      fileMetadata.content_hash = entry["content_hash"];
      fileMetadata.fileSize = entry["size"];
      fileMetadata.rev = entry["rev"];
      fileMetadata.latest_rev = entry["rev"];
      fileMetadata.storeToDatabase(db_);
      Logger::info("Updated metadata for unchanged file: " + fileMetadata.getFileName());
    }
  }

  // Resolve file conflicts using a three-way merge if possible.
  void resolveFileConflict(const nlohmann::json &entry, File_Metadata &fileMetadata,
                           const std::string &localContent, const std::string &localHash,
                           const std::string &relativePath)
  {
    std::string serverContent = downloadFileContent(fileMetadata.getFileName());
    std::string baseContent = fileMetadata.file_content;
    std::string mergedContent;
    bool mergePossible = MergeLib::three_way_merge(baseContent, localContent, serverContent, mergedContent);

    if (mergePossible)
    {
      Logger::info("Merge successful for: " + fileMetadata.getFileName());
      fileMetadata.file_content = mergedContent;
      FileSystemUtil::createFile(relativePath, monitoringDirectory_, mergedContent);
      auto response = dropboxClient_->modifyFile(fileMetadata.getFileName(), mergedContent, fileMetadata.rev);
      if (response.responseCode == 200)
      {
        Logger::info("File modified on Dropbox: " + fileMetadata.getFileName());
        auto responseJson = nlohmann::json::parse(response.content);
        fileMetadata.content_hash = responseJson["content_hash"];
        fileMetadata.fileSize = responseJson["size"];
        fileMetadata.rev = responseJson["rev"];
        fileMetadata.latest_rev = responseJson["rev"];
        fileMetadata.storeToDatabase(db_);
      }
      else
      {
        Logger::error("Failed to modify file on Dropbox: " + response.content);
      }
    }
    else
    {
      Logger::error("Merge failed for: " + fileMetadata.getFileName());
      handleMergeConflict(entry, fileMetadata, localContent, localHash, relativePath, serverContent);
    }
  }

  // Handle merge conflicts by creating a conflict file and updating the original.
  void handleMergeConflict(const nlohmann::json &entry, File_Metadata &fileMetadata,
                           const std::string &localContent, const std::string &localHash,
                           const std::string &relativePath, const std::string &serverContent)
  {
    std::string conflictFileName;
    size_t dotPos = relativePath.find_last_of('.');
    if (dotPos != std::string::npos && dotPos > relativePath.find_last_of('/'))
    {
      conflictFileName = relativePath.substr(0, dotPos) + "$conflict$" + relativePath.substr(dotPos);
    }
    else
    {
      conflictFileName = relativePath + "$conflict$";
    }
    FileSystemUtil::createFile(conflictFileName, monitoringDirectory_, localContent);
    Logger::info("Conflict file created: " + conflictFileName);
    FileSystemUtil::createFile(relativePath, monitoringDirectory_, serverContent);
    Logger::info("Replaced file with server content: " + relativePath);

    std::string conflictFilePath = user_ + "/" + conflictFileName;

    fileMetadata.file_content = serverContent;
    fileMetadata.content_hash = entry["content_hash"];
    fileMetadata.fileSize = entry["size"];
    fileMetadata.rev = entry["rev"];
    fileMetadata.latest_rev = entry["rev"];
    fileMetadata.storeToDatabase(db_);
    updateDirectoryMetadata(fileMetadata.getFileName());

    File_Metadata conflictMetadata;
    conflictMetadata.setFileName(conflictFilePath);
    conflictMetadata.content_hash = localHash;
    conflictMetadata.fileSize = localContent.size();
    auto conflictResponse = dropboxClient_->createFile(conflictFilePath, localContent);
    if (conflictResponse.responseCode == 200)
    {
      auto conflictResponseJson = nlohmann::json::parse(conflictResponse.content);
      conflictMetadata.content_hash = conflictResponseJson["content_hash"];
      conflictMetadata.fileSize = conflictResponseJson["size"];
      conflictMetadata.rev = conflictResponseJson["rev"];
      conflictMetadata.latest_rev = conflictResponseJson["rev"];
      conflictMetadata.storeToDatabase(db_);
      updateDirectoryMetadata(conflictFilePath);
      Logger::info("Conflict file uploaded to Dropbox: " + conflictFilePath);
    }
    else
    {
      Logger::error("Failed to upload conflict file to Dropbox: " + conflictResponse.content);
    }
  }

  // Download file content from Dropbox.
  std::string downloadFileContent(const std::string &filePath)
  {
    auto response = dropboxClient_->readFile(filePath);
    Logger::info("Download response for '" + filePath + "': " + std::to_string(response.responseCode));
    Logger::info("Download content: " + response.content);
    return (response.responseCode == 200) ? response.content : "";
  }

  // Update directory metadata to include the given file.
  void updateDirectoryMetadata(const std::string &filePath)
  {
    Directory_Metadata dirMetadata;
    std::string dirPath = filePath.substr(0, filePath.find_last_of('/')) + "/";
    Logger::info("Updating directory metadata for: " + dirPath);
    dirMetadata.setDirectoryName(dirPath);
    if (!dirMetadata.loadFromDatabase(db_))
    {
      Logger::info("Directory metadata not found; storing new entry: " + dirPath);
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

// Simplified bootup function that configures and calls the synchronization manager.
void bootup_2(std::shared_ptr<rocksdb::DB> db,
              std::shared_ptr<DropboxClient> dropboxClient,
              nlohmann::json &config)
{
  if (!config.contains("user") || !config.contains("monitoring_directory"))
  {
    Logger::error("Configuration missing required fields: 'user' or 'monitoring_directory'");
    return;
  }
  std::string user = config["user"];
  std::string monitoringDirectory = config["monitoring_directory"];
  Logger::info("Initializing sync for user: " + user);

  // Ensure the user's Dropbox folder exists.
  DropboxAPI::ensureUserFolder(dropboxClient, user);
  // Retrieve Dropbox folder content.
  nlohmann::json content = DropboxAPI::getFolderContent(dropboxClient, user);
  if (content.is_null() || !content.contains("entries"))
  {
    Logger::error("Invalid Dropbox folder content for user: " + user);
    return;
  }
  Logger::info("Dropbox folder content: " + content.dump());

  DropboxSyncManager syncManager(db, dropboxClient, user, monitoringDirectory);
  syncManager.synchronize(content["entries"]);
}
