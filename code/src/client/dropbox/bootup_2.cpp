#include "dropbox_client.h"
#include <memory>
#include <nlohmann/json.hpp>
#include <rocksdb/db.h>
#include <string>
#include <vector>
#include "../hash/dropbox_content_hash.hpp"
#include "../merge/three_way_merge.hpp"

namespace DropboxAPI
{

  // Group Dropbox-related operations together.
  void ensureUserFolder(std::shared_ptr<DropboxClient> dropboxClient, const std::string &user)
  {
    Logger::debug("Ensuring Dropbox folder for user: " + user);
    auto metadata = dropboxClient->getMetadata(user);
    Logger::info("User folder metadata response code: " + std::to_string(metadata.responseCode));
    Logger::info("User folder metadata content: " + metadata.content);
    if (metadata.responseCode == 409)
    {
      Logger::info("User folder does not exist, attempting to create folder: " + user);
      auto createResponse = dropboxClient->createFolder(user);
      if (createResponse.responseCode == 200)
      {
        Logger::info("Successfully created folder on Dropbox: " + user);
      }
      else
      {
        Logger::error("Failed to create folder on Dropbox: " + createResponse.content);
      }
    }
    else
    {
      Logger::debug("User folder already exists: " + user);
    }
  }

  nlohmann::json getFolderContent(std::shared_ptr<DropboxClient> dropboxClient, const std::string &user)
  {
    Logger::debug("Fetching folder content for user: " + user);
    auto response = dropboxClient->listContent(user + "/");
    if (response.responseCode != 200)
    {
      Logger::error("Failed to list content on Dropbox: " + response.content);
      return nlohmann::json();
    }
    Logger::debug("Successfully retrieved folder content for user: " + user);
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
      : db_(db), dropboxClient_(dropboxClient), user_(user), monitoringDirectory_(monitoringDirectory)
  {
    Logger::debug("Initialized DropboxSyncManager for user: " + user);
  }

  // Synchronize directories and files based on entries from Dropbox.
  void synchronize(const nlohmann::json &entries)
  {
    Logger::info("Starting Dropbox synchronization process.");
    processEntries(entries, "folder");
    processEntries(entries, "file");
    Logger::info("Completed Dropbox synchronization process.");
  }

private:
  // Process entries based on tag (either "folder" or "file").
  void processEntries(const nlohmann::json &entries, const std::string &tag)
  {
    Logger::debug("Processing entries for tag: " + tag);
    for (const auto &entry : entries)
    {
      if (entry[".tag"] == tag)
      {
        Logger::debug("Entry with tag '" + tag + "' found: " + entry.dump());
        (tag == "folder") ? processDirectoryEntry(entry) : processFileEntry(entry);
      }
    }
    Logger::debug("Finished processing entries for tag: " + tag);
  }

  // Process a directory entry.
  void processDirectoryEntry(const nlohmann::json &entry)
  {
    Logger::debug("Processing directory entry.");
    std::string fullPath = entry["path_display"];
    if (fullPath.back() != '/')
      fullPath += "/";
    Logger::info("Processing directory: " + fullPath);

    Directory_Metadata metadata;
    metadata.setDirectoryName(fullPath);
    if (!metadata.loadFromDatabase(db_))
    {
      Logger::info("Directory not found in database: " + fullPath);
      if (metadata.storeToDatabase(db_))
      {
        Logger::info("Stored new directory metadata for: " + fullPath);
      }
      else
      {
        Logger::error("Failed to store new directory metadata for: " + fullPath);
      }
      std::string relativePath = fullPath.substr(user_.length() + 1);
      Logger::debug("Creating local directory at: " + monitoringDirectory_ + "/" + relativePath);
      FileSystemUtil::createDirectory(relativePath, monitoringDirectory_);
    }
    else
    {
      Logger::debug("Directory metadata exists for: " + fullPath);
    }
  }

  // Process a file entry by determining if it is new or updated.
  void processFileEntry(const nlohmann::json &entry)
  {
    Logger::debug("Processing file entry.");
    std::string fullPath = entry["path_display"];
    Logger::info("Processing file: " + fullPath);

    File_Metadata fileMetadata;
    fileMetadata.setFileName(fullPath);
    std::string relativePath = fullPath.substr(user_.length() + 1);

    if (!fileMetadata.loadFromDatabase(db_))
    {
      Logger::debug("File not found in database; handling as new file: " + fullPath);
      handleNewFile(entry, fileMetadata, relativePath);
    }
    else
    {
      Logger::debug("File exists in database; handling as existing file: " + fullPath);
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

    Logger::debug("Downloading new file content for: " + fileMetadata.getFileName());
    std::string content = downloadFileContent(fileMetadata.getFileName());
    fileMetadata.file_content = content;
    Logger::debug("Creating local file for new file at: " + monitoringDirectory_ + "/" + relativePath);
    FileSystemUtil::createFile(relativePath, monitoringDirectory_, content);
    if (fileMetadata.storeToDatabase(db_))
    {
      Logger::info("Stored new file metadata in database for: " + fileMetadata.getFileName());
    }
    else
    {
      Logger::error("Failed to store new file metadata in database for: " + fileMetadata.getFileName());
    }
    updateDirectoryMetadata(fileMetadata.getFileName());
  }

  // Handle existing files by checking for changes and possible merge conflicts.
  void handleExistingFile(const nlohmann::json &entry, File_Metadata &fileMetadata, const std::string &relativePath)
  {
    Logger::info("Existing file found: " + fileMetadata.getFileName());
    auto localContent = FileSystemUtil::readFileContent(relativePath, monitoringDirectory_);
    Logger::debug("Local content read for file: " + fileMetadata.getFileName());
    auto localHash = dropbox::compute_content_hash(localContent);
    Logger::info("Local hash: " + localHash);
    std::string serverHash = entry["content_hash"];
    Logger::info("Server hash: " + serverHash);

    if (localHash != serverHash)
    {
      Logger::info("Hash mismatch detected for file: " + fileMetadata.getFileName());
      resolveFileConflict(entry, fileMetadata, localContent, localHash, relativePath);
    }
    else
    {
      Logger::debug("No changes detected for file: " + fileMetadata.getFileName());
      fileMetadata.content_hash = entry["content_hash"];
      fileMetadata.fileSize = entry["size"];
      fileMetadata.rev = entry["rev"];
      fileMetadata.latest_rev = entry["rev"];
      if (fileMetadata.storeToDatabase(db_))
      {
        Logger::info("Updated metadata for unchanged file: " + fileMetadata.getFileName());
      }
      else
      {
        Logger::error("Failed to update metadata for unchanged file: " + fileMetadata.getFileName());
      }
    }
  }

  // Resolve file conflicts using a three-way merge if possible.
  void resolveFileConflict(const nlohmann::json &entry, File_Metadata &fileMetadata,
                           const std::string &localContent, const std::string &localHash,
                           const std::string &relativePath)
  {
    Logger::debug("Attempting to resolve file conflict for: " + fileMetadata.getFileName());
    std::string serverContent = downloadFileContent(fileMetadata.getFileName());
    std::string baseContent = fileMetadata.file_content;
    std::string mergedContent;
    bool mergePossible = MergeLib::three_way_merge(baseContent, localContent, serverContent, mergedContent);

    if (mergePossible)
    {
      Logger::info("Merge successful for: " + fileMetadata.getFileName());
      fileMetadata.file_content = mergedContent;
      Logger::debug("Updating local file with merged content for: " + fileMetadata.getFileName());
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
        if (fileMetadata.storeToDatabase(db_))
        {
          Logger::info("Stored updated file metadata after merge for: " + fileMetadata.getFileName());
        }
        else
        {
          Logger::error("Failed to store updated file metadata after merge for: " + fileMetadata.getFileName());
        }
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
    Logger::debug("Handling merge conflict for file: " + fileMetadata.getFileName());
    std::string conflictFileName;
    size_t dotPos = relativePath.find_last_of('.');
    size_t slashPos = relativePath.find_last_of('/');

    if (dotPos != std::string::npos && (slashPos == std::string::npos || dotPos > slashPos))
    {
      conflictFileName = relativePath.substr(0, dotPos) + "$conflict$" + relativePath.substr(dotPos);
    }
    else
    {
      conflictFileName = relativePath + "$conflict$";
    }
    Logger::info("Conflict file name generated: " + conflictFileName);
    FileSystemUtil::createFile(conflictFileName, monitoringDirectory_, localContent);
    Logger::info("Conflict file created locally: " + conflictFileName);
    FileSystemUtil::createFile(relativePath, monitoringDirectory_, serverContent);
    Logger::info("Replaced file with server content locally: " + relativePath);

    std::string conflictFilePath = user_ + "/" + conflictFileName;

    fileMetadata.file_content = serverContent;
    fileMetadata.content_hash = entry["content_hash"];
    fileMetadata.fileSize = entry["size"];
    fileMetadata.rev = entry["rev"];
    fileMetadata.latest_rev = entry["rev"];
    if (fileMetadata.storeToDatabase(db_))
    {
      Logger::info("Stored updated metadata for file after conflict resolution: " + fileMetadata.getFileName());
    }
    else
    {
      Logger::error("Failed to store updated metadata for file after conflict resolution: " + fileMetadata.getFileName());
    }
    updateDirectoryMetadata(fileMetadata.getFileName());

    File_Metadata conflictMetadata;
    conflictMetadata.setFileName(conflictFilePath);
    conflictMetadata.content_hash = localHash;
    conflictMetadata.fileSize = localContent.size();
    auto conflictResponse = dropboxClient_->createFile(conflictFilePath, localContent);
    if (conflictResponse.responseCode == 200)
    {
      Logger::info("Conflict file uploaded to Dropbox: " + conflictFilePath);
      auto conflictResponseJson = nlohmann::json::parse(conflictResponse.content);
      conflictMetadata.content_hash = conflictResponseJson["content_hash"];
      conflictMetadata.fileSize = conflictResponseJson["size"];
      conflictMetadata.rev = conflictResponseJson["rev"];
      conflictMetadata.latest_rev = conflictResponseJson["rev"];
      if (conflictMetadata.storeToDatabase(db_))
      {
        Logger::info("Stored conflict file metadata in database: " + conflictFilePath);
      }
      else
      {
        Logger::error("Failed to store conflict file metadata in database: " + conflictFilePath);
      }
      updateDirectoryMetadata(conflictFilePath);
    }
    else
    {
      Logger::error("Failed to upload conflict file to Dropbox: " + conflictResponse.content);
    }
  }

  // Download file content from Dropbox.
  std::string downloadFileContent(const std::string &filePath)
  {
    Logger::debug("Downloading file content for: " + filePath);
    auto response = dropboxClient_->readFile(filePath);
    Logger::info("Download response for '" + filePath + "': " + std::to_string(response.responseCode));
    Logger::debug("Download content: " + response.content);
    if (response.responseCode != 200)
      Logger::error("Failed to download file content for: " + filePath);
    return (response.responseCode == 200) ? response.content : "";
  }

  // Update directory metadata to include the given file.
  void updateDirectoryMetadata(const std::string &filePath)
  {
    Logger::debug("Updating directory metadata for file: " + filePath);
    Directory_Metadata dirMetadata;
    std::string dirPath = filePath.substr(0, filePath.find_last_of('/')) + "/";
    Logger::info("Updating directory metadata for: " + dirPath);
    dirMetadata.setDirectoryName(dirPath);
    if (!dirMetadata.loadFromDatabase(db_))
    {
      Logger::info("Directory metadata not found; storing new entry: " + dirPath);
      if (!dirMetadata.storeToDatabase(db_))
      {
        Logger::error("Failed to store new directory metadata for: " + dirPath);
      }
    }
    dirMetadata.files.push_back(filePath);
    if (dirMetadata.storeToDatabase(db_))
    {
      Logger::info("Directory metadata updated with file: " + filePath);
    }
    else
    {
      Logger::error("Failed to update directory metadata with file: " + filePath);
    }
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
  Logger::info("Starting Dropbox synchronization bootup.");
  if (!config.contains("user") || !config.contains("monitoring_directory"))
  {
    Logger::error("Configuration missing required fields: 'user' or 'monitoring_directory'");
    return;
  }
  std::string user = config["user"];
  std::string monitoringDirectory = config["monitoring_directory"];
  Logger::debug("Bootup config - User: " + user + ", Monitoring Directory: " + monitoringDirectory);
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
  Logger::debug("Dropbox folder content retrieved: " + content.dump());
  Logger::info("Dropbox folder content successfully fetched.");

  DropboxSyncManager syncManager(db, dropboxClient, user, monitoringDirectory);
  syncManager.synchronize(content["entries"]);
  Logger::info("Dropbox synchronization bootup completed.");
}
