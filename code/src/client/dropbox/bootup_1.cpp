#include "../logger/logger.h"
#include "dropbox_client.h"
#include <filesystem>
#include <iostream>
#include <memory>
#include <nlohmann/json.hpp>
#include <rocksdb/db.h>
#include <set>
#include <string>
#include <vector>

namespace fs = std::filesystem;

class DBSynchronizer
{
public:
  DBSynchronizer(const std::string &basePath, const std::string &user, std::shared_ptr<rocksdb::DB> db)
      : basePath_(basePath), user_(user), db_(db) {}

  // Initiates synchronization between the filesystem and the database.
  void sync()
  {
    Logger::info("Starting synchronization process.");
    if (!fs::exists(basePath_))
    {
      Logger::error("The monitoring directory does not exist: " + basePath_);
      return;
    }
    Logger::debug("Monitoring directory exists: " + basePath_);
    syncDirectory(fs::path(basePath_));
    Logger::info("Finished synchronization process.");
  }

private:
  std::string basePath_;
  std::string user_;
  std::shared_ptr<rocksdb::DB> db_;

  // Helper to delete a key from the database with logging.
  bool deleteKey(const std::string &key, const std::string &type)
  {
    Logger::debug("Attempting to delete " + type + " from database: " + key);
    auto deleteStatus = db_->Delete(rocksdb::WriteOptions(), key);
    if (!deleteStatus.ok())
    {
      Logger::error("Failed to delete " + type + " from database: " + key);
      return false;
    }
    else
    {
      Logger::info("Deleted " + type + " from database: " + key);
      return true;
    }
  }

  // Recursively synchronizes a directory.
  void syncDirectory(const fs::path &currentPath)
  {
    Logger::debug("Synchronizing directory: " + currentPath.string());
    if (!fs::exists(currentPath))
    {
      Logger::error("Path does not exist: " + currentPath.string());
      return;
    }
    if (!fs::is_directory(currentPath))
    {
      Logger::warning("Path is not a directory, skipping: " + currentPath.string());
      return;
    }

    std::set<std::string> fsChildrenKeys;
    std::vector<std::pair<bool, std::string>> childrenDetails; // <isDirectory, relativePath>

    auto relCurrent = fs::relative(currentPath, basePath_);
    std::string currentDirKey = (relCurrent.string() == ".")
                                    ? user_ + "/"
                                    : user_ + "/" + relCurrent.string() + "/";
    Logger::debug("Current directory key: " + currentDirKey);
    fsChildrenKeys.insert(currentDirKey);

    // Process each filesystem entry.
    for (const auto &entry : fs::directory_iterator(currentPath))
    {
      bool isDir = fs::is_directory(entry);
      fs::path relChild = fs::relative(entry.path(), basePath_);
      std::string childKey = user_ + "/" + relChild.string() + (isDir ? "/" : "");
      Logger::info("Processing entry: " + childKey);
      fsChildrenKeys.insert(childKey);
      childrenDetails.push_back({isDir, relChild.string()});
      if (isDir)
      {
        Logger::debug("Recursing into directory: " + entry.path().string());
        syncDirectory(entry.path());
      }
      else
      {
        Logger::debug("File detected: " + entry.path().string());
      }
    }

    // Load the metadata for the current directory from the database.
    Logger::debug("Loading metadata for directory: " + currentDirKey);
    Directory_Metadata metadata;
    metadata.setDirectoryName(currentDirKey);
    if (metadata.loadFromDatabase(db_))
    {
      Logger::debug("Metadata loaded successfully for: " + currentDirKey);
      // Clean up file entries that no longer exist on disk.
      for (const auto &file : metadata.files)
      {
        if (fsChildrenKeys.find(file) == fsChildrenKeys.end())
        {
          Logger::warning("File not found in filesystem: " + file);
          deleteKey(file, "file");
        }
      }
      // Clean up directory entries that no longer exist on disk.
      for (const auto &dir : metadata.directories)
      {
        if (fsChildrenKeys.find(dir) == fsChildrenKeys.end())
        {
          Logger::warning("Directory not found in filesystem: " + dir);
          deleteDirectoryInDB(dir);
        }
      }
    }
    else
    {
      Logger::warning("No metadata found for directory: " + currentDirKey + ". Creating new metadata.");
    }

    // Clear metadata lists and update with current filesystem details.
    metadata.directories.clear();
    metadata.files.clear();

    for (const auto &child : childrenDetails)
    {
      if (child.first)
      {
        std::string dirKey = user_ + "/" + child.second + "/";
        metadata.directories.push_back(dirKey);
      }
      else
      {
        std::string fileKey = user_ + "/" + child.second;
        metadata.files.push_back(fileKey);
        updateFileMetadata(fileKey);
      }
    }

    if (!metadata.storeToDatabase(db_))
    {
      Logger::error("Failed to store directory metadata in database: " + currentDirKey);
    }
    else
    {
      Logger::info("Stored directory metadata in database: " + currentDirKey);
    }
  }

  // Helper to update file metadata.
  void updateFileMetadata(const std::string &fileKey)
  {
    Logger::debug("Updating file metadata for: " + fileKey);
    File_Metadata fileMetadata;
    fileMetadata.setFileName(fileKey);
    if (!fileMetadata.loadFromDatabase(db_))
    {
      Logger::debug("File not found in database, creating new metadata for: " + fileKey);
      fileMetadata.storeToDatabase(db_);
      Logger::info("Stored new file metadata in database: " + fileKey);
    }
    else
    {
      Logger::debug("File metadata exists for: " + fileKey);
    }
  }

  // Deletes a directory and all its children from the database.
  void deleteDirectoryInDB(const std::string &dirKey)
  {
    Logger::debug("Deleting directory in DB: " + dirKey);
    Directory_Metadata metadata;
    metadata.setDirectoryName(dirKey);
    if (!metadata.loadFromDatabase(db_))
    {
      Logger::error("Directory not found in database: " + dirKey);
      return;
    }
    // Delete the directory entry.
    deleteKey(dirKey, "directory");
    // Delete all file entries.
    for (const auto &fileKey : metadata.files)
    {
      deleteKey(fileKey, "file");
    }
    // Recursively delete any subdirectories.
    for (const auto &subDirKey : metadata.directories)
    {
      deleteDirectoryInDB(subDirKey);
    }
  }
};

// bootup_1 is the only externally exposed function.
void bootup_1(const nlohmann::json &config, std::shared_ptr<rocksdb::DB> db)
{
  Logger::info("Bootup process started.");
  if (config.contains("monitoring_directory") && config.contains("user"))
  {
    std::string monitoringDirectory = config["monitoring_directory"];
    std::string user = config["user"];

    Logger::debug("Configuration - Monitoring Directory: " + monitoringDirectory + ", User: " + user);
    DBSynchronizer synchronizer(monitoringDirectory, user, db);
    synchronizer.sync();
  }
  else
  {
    Logger::error("Invalid configuration file: missing 'monitoring_directory' or 'user'.");
    std::cerr << "Invalid configuration file." << std::endl;
  }
  Logger::info("Bootup process completed.");
}
