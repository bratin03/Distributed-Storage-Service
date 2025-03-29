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
  DBSynchronizer(const std::string &basePath, const std::string &user, std::shared_ptr<rocksdb::DB> db) : basePath_(basePath), user_(user), db_(db) {}

  // Initiates synchronization between the filesystem and the database.
  void sync()
  {
    if (!fs::exists(basePath_))
    {
      Logger::error("The monitoring directory does not exist: " + basePath_);
      return;
    }
    syncDirectory(fs::path(basePath_));
  }

private:
  std::string basePath_;
  std::string user_;
  std::shared_ptr<rocksdb::DB> db_;

  // Recursively synchronizes a directory.
  void syncDirectory(const fs::path &currentPath)
  {
    if (!fs::exists(currentPath))
    {
      Logger::error("Path does not exist: " + currentPath.string());
      return;
    }
    if (!fs::is_directory(currentPath))
      return;

      
    std::set<std::string> fsChildrenKeys;
    std::vector<std::pair<bool, std::string>>
        childrenDetails; // <isDirectory, relativePath>

    auto relCurrent = fs::relative(currentPath, basePath_);
    std::string currentDirKey = (relCurrent.string() == ".")
                                    ? user_ + "/"
                                    : user_ + "/" + relCurrent.string() + "/";
    fsChildrenKeys.insert(currentDirKey);

    // Process each filesystem entry.
    for (const auto &entry : fs::directory_iterator(currentPath))
    {
      bool isDir = fs::is_directory(entry);
      fs::path relChild = fs::relative(entry.path(), basePath_);
      std::string childKey =
          user_ + "/" + relChild.string() + (isDir ? "/" : "");
      Logger::info("Processing entry: " + childKey);
      fsChildrenKeys.insert(childKey);
      childrenDetails.push_back({isDir, relChild.string()});
      if (isDir)
      {
        syncDirectory(entry.path());
      }
    }

    // Load the metadata for the current directory from the database.
    Directory_Metadata metadata;
    metadata.setDirectoryName(currentDirKey);
    if (metadata.loadFromDatabase(db_))
    {
      // Clean up file entries that no longer exist on disk.
      for (const auto &file : metadata.files)
      {
        if (fsChildrenKeys.find(file) == fsChildrenKeys.end())
        {
          Logger::error("File not found in filesystem: " + file);
          deleteFileInDB(file);
        }
      }
      // Clean up directory entries that no longer exist on disk.
      for (const auto &dir : metadata.directories)
      {
        if (fsChildrenKeys.find(dir) == fsChildrenKeys.end())
        {
          Logger::error("Directory not found in filesystem: " + dir);
          deleteDirectoryInDB(dir);
        }
      }
    }

    metadata.directories.clear();
    metadata.files.clear();

    // Update metadata with current filesystem details.
    for (const auto &child : childrenDetails)
    {
      if (child.first)
      {
        metadata.directories.push_back(user_ + "/" + child.second + "/");
      }
      else
      {
        metadata.files.push_back(user_ + "/" + child.second);
        File_Metadata fileMetadata;
        fileMetadata.setFileName(user_ + "/" + child.second);
        if (!fileMetadata.loadFromDatabase(db_))
        {
          Logger::info("File not found in database: " +
                       fileMetadata.getFileName());
          fileMetadata.storeToDatabase(db_);
          Logger::info("Stored new file metadata in database: " +
                       fileMetadata.getFileName());
        }
      }
    }
    if (!metadata.storeToDatabase(db_))
    {
      Logger::error("Failed to store directory metadata in database: " +
                    currentDirKey);
    }
    else
    {
      Logger::info("Stored directory metadata in database: " + currentDirKey);
    }
  }

  // Deletes a directory and all its children from the database.
  void deleteDirectoryInDB(const std::string &dirKey)
  {
    Directory_Metadata metadata;
    metadata.setDirectoryName(dirKey);
    if (!metadata.loadFromDatabase(db_))
    {
      Logger::error("Directory not found in database: " + dirKey);
      return;
    }
    // Delete the directory entry.
    auto deleteStatus = db_->Delete(rocksdb::WriteOptions(), dirKey);
    if (!deleteStatus.ok())
    {
      Logger::error("Failed to delete directory from database: " + dirKey);
    }
    else
    {
      Logger::info("Deleted directory from database: " + dirKey);
    }
    // Delete all file entries.
    for (const auto &fileKey : metadata.files)
    {
      deleteFileInDB(fileKey);
    }
    // Recursively delete any subdirectories.
    for (const auto &subDirKey : metadata.directories)
    {
      deleteDirectoryInDB(subDirKey);
    }
  }

  // Removes a file entry from the database.
  void deleteFileInDB(const std::string &fileKey)
  {
    auto deleteStatus = db_->Delete(rocksdb::WriteOptions(), fileKey);
    if (!deleteStatus.ok())
    {
      Logger::error("Failed to delete file from database: " + fileKey);
    }
    else
    {
      Logger::info("Deleted file from database: " + fileKey);
    }
  }
};

// Bootup function to initialize synchronization using a JSON configuration.
void bootup_1(const nlohmann::json &config, std::shared_ptr<rocksdb::DB> db)
{
  if (config.contains("monitoring_directory") && config.contains("user"))
  {
    std::string monitoringDirectory = config["monitoring_directory"];
    std::string user = config["user"];

    DBSynchronizer synchronizer(monitoringDirectory, user, db);
    synchronizer.sync();
  }
  else
  {
    std::cerr << "Invalid configuration file." << std::endl;
  }
}
