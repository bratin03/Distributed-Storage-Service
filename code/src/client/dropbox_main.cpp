#include "dropbox/dropbox_client.h"
#include <fstream>
#include <nlohmann/json.hpp>
#include <string>
#include <thread>
#include <filesystem>
#include <mutex>
#include <queue>
#include <algorithm>
#include <stdexcept>
#include <rocksdb/db.h>

const std::string config_path = "config.json";
std::string user;
std::string base_path;

namespace fs = std::filesystem;

//================== File System Utilities ==================
namespace FileUtils
{

  // Create a directory relative to a base path.
  bool createDirectory(const std::string &relativePath, const fs::path &basePath)
  {
    fs::path fullPath = basePath / relativePath;
    std::error_code ec;
    if (!fs::create_directories(fullPath, ec))
    {
      if (ec)
      {
        Logger::error("Failed to create directory: " + fullPath.string() + " (" + ec.message() + ")");
        return false;
      }
    }
    Logger::info("Created directory: " + fullPath.string());
    return true;
  }

  // Delete a file or directory from the filesystem.
  void deleteFileSystemEntry(const std::string &name, const fs::path &basePath)
  {
    fs::path fullPath = basePath / name;
    std::error_code ec;

    if (!fs::exists(fullPath, ec))
    {
      Logger::error("Path does not exist: " + fullPath.string());
      return;
    }

    if (fs::is_directory(fullPath, ec))
    {
      auto count = fs::remove_all(fullPath, ec);
      if (ec)
      {
        Logger::error("Failed to delete directory: " + fullPath.string() + " (" + ec.message() + ")");
      }
      else
      {
        Logger::info("Deleted directory: " + fullPath.string() + " (" + std::to_string(count) + " items removed)");
      }
    }
    else
    {
      bool removed = fs::remove(fullPath, ec);
      if (!removed || ec)
      {
        Logger::error("Failed to delete file: " + fullPath.string() + " (" + ec.message() + ")");
      }
      else
      {
        Logger::info("Deleted file: " + fullPath.string());
      }
    }
  }

} // namespace FileUtils

//================== Database Metadata Utilities ==================
namespace DBUtils
{

  // Deletes an entry from the database with logging.
  bool deleteDBEntry(std::shared_ptr<rocksdb::DB> db, const std::string &key, const std::string &type)
  {
    Logger::debug("Attempting to delete " + type + " from database: " + key);
    auto deleteStatus = db->Delete(rocksdb::WriteOptions(), key);
    if (!deleteStatus.ok())
    {
      Logger::error("Failed to delete " + type + " from database: " + key);
      return false;
    }
    Logger::info("Deleted " + type + " from database: " + key);
    return true;
  }

  // Extract the parent directory from a key.
  // If isDirectory is true, removes a trailing slash before processing.
  std::string getParentDirectory(const std::string &key, bool isDirectory = false)
  {
    std::string keyCopy = key;
    if (isDirectory && !keyCopy.empty() && keyCopy.back() == '/')
      keyCopy.pop_back();
    size_t lastSlash = keyCopy.find_last_of('/');
    if (lastSlash == std::string::npos)
      return "";
    std::string parentDir = keyCopy.substr(0, lastSlash);
    if (parentDir.empty() || parentDir.back() != '/')
      parentDir += "/";
    return parentDir;
  }

  // Update parent directory metadata: add a file entry.
  bool addFileToParentDirectory(std::shared_ptr<rocksdb::DB> db, const std::string &fileKey)
  {
    std::string parentDir = getParentDirectory(fileKey);
    if (parentDir.empty())
    {
      Logger::error("Invalid file key: " + fileKey);
      return false;
    }
    Directory_Metadata parentMeta;
    parentMeta.setDirectoryName(parentDir);
    if (!parentMeta.loadFromDatabase(db))
    {
      Logger::error("Parent directory not found in database: " + parentDir);
      return false;
    }
    // Only add if not already present.
    if (std::find(parentMeta.files.begin(), parentMeta.files.end(), fileKey) == parentMeta.files.end())
    {
      parentMeta.files.push_back(fileKey);
      if (!parentMeta.storeToDatabase(db))
      {
        Logger::error("Failed to update parent directory metadata in database: " + parentDir);
        return false;
      }
      Logger::info("Added file to parent directory: " + fileKey);
    }
    else
    {
      Logger::warning("File already exists in parent directory's list: " + fileKey);
    }
    return true;
  }

  // Update parent directory metadata: remove a file entry.
  bool removeFileFromParentDirectory(std::shared_ptr<rocksdb::DB> db, const std::string &fileKey)
  {
    std::string parentDir = getParentDirectory(fileKey);
    if (parentDir.empty())
    {
      Logger::error("Invalid file key: " + fileKey);
      return false;
    }
    Directory_Metadata parentMeta;
    parentMeta.setDirectoryName(parentDir);
    if (!parentMeta.loadFromDatabase(db))
    {
      Logger::error("Parent directory not found in database: " + parentDir);
      return false;
    }
    auto it = std::find(parentMeta.files.begin(), parentMeta.files.end(), fileKey);
    if (it != parentMeta.files.end())
    {
      parentMeta.files.erase(it);
      if (!parentMeta.storeToDatabase(db))
      {
        Logger::error("Failed to update parent directory metadata in database: " + parentDir);
        return false;
      }
      Logger::info("Removed file from parent directory: " + fileKey);
    }
    else
    {
      Logger::warning("File not found in parent directory's list: " + fileKey);
    }
    return true;
  }

  // Update parent directory metadata: add a directory entry.
  bool addDirectoryToParent(std::shared_ptr<rocksdb::DB> db, const std::string &dirKey)
  {
    std::string parentDir = getParentDirectory(dirKey, true);
    if (parentDir.empty())
    {
      Logger::error("Invalid directory key: " + dirKey);
      return false;
    }
    Directory_Metadata parentMeta;
    parentMeta.setDirectoryName(parentDir);
    if (!parentMeta.loadFromDatabase(db))
    {
      Logger::error("Parent directory not found in database: " + parentDir);
      return false;
    }
    // Only add if not already present.
    if (std::find(parentMeta.directories.begin(), parentMeta.directories.end(), dirKey) == parentMeta.directories.end())
    {
      parentMeta.directories.push_back(dirKey);
      if (!parentMeta.storeToDatabase(db))
      {
        Logger::error("Failed to update parent directory metadata in database: " + parentDir);
        return false;
      }
      Logger::info("Added directory to parent directory: " + dirKey);
    }
    else
    {
      Logger::warning("Directory already exists in parent directory's list: " + dirKey);
    }
    return true;
  }

  // Update parent directory metadata: remove a directory entry.
  bool removeDirectoryFromParent(std::shared_ptr<rocksdb::DB> db, const std::string &dirKey)
  {
    std::string parentDir = getParentDirectory(dirKey, true);
    if (parentDir.empty())
    {
      Logger::error("Invalid directory key: " + dirKey);
      return false;
    }
    Directory_Metadata parentMeta;
    parentMeta.setDirectoryName(parentDir);
    if (!parentMeta.loadFromDatabase(db))
    {
      Logger::error("Parent directory not found in database: " + parentDir);
      return false;
    }
    auto it = std::find(parentMeta.directories.begin(), parentMeta.directories.end(), dirKey);
    if (it != parentMeta.directories.end())
    {
      parentMeta.directories.erase(it);
      if (!parentMeta.storeToDatabase(db))
      {
        Logger::error("Failed to update parent directory metadata in database: " + parentDir);
        return false;
      }
      Logger::info("Removed directory from parent directory: " + dirKey);
    }
    else
    {
      Logger::warning("Directory not found in parent directory's list: " + dirKey);
    }
    return true;
  }

  // Recursively delete a directory and all its children from the database.
  void deleteDirectoryRecursively(std::shared_ptr<rocksdb::DB> db, const std::string &dirKey)
  {
    Logger::debug("Deleting directory in DB: " + dirKey);
    Directory_Metadata meta;
    meta.setDirectoryName(dirKey);
    if (!meta.loadFromDatabase(db))
    {
      Logger::error("Directory not found in database: " + dirKey);
      return;
    }
    // Delete this directory entry.
    deleteDBEntry(db, dirKey, "directory");
    // Delete all file entries.
    for (const auto &fileKey : meta.files)
    {
      deleteDBEntry(db, fileKey, "file");
    }
    // Recursively delete any subdirectories.
    for (const auto &subDirKey : meta.directories)
    {
      deleteDirectoryRecursively(db, subDirKey);
    }
  }

} // namespace DBUtils

//================== Event Handling Utilities ==================
namespace EventHandler
{

  // Handle update events (currently only folder creation is implemented).
  void handleUpdateEvent(const nlohmann::json &event, std::shared_ptr<rocksdb::DB> db)
  {
    if (event["item_type"] == "folder")
    {
      std::string fullPath = event["full_path"].get<std::string>();
      fullPath += "/";
      Directory_Metadata dirMeta;
      dirMeta.setDirectoryName(fullPath);
      if (!dirMeta.loadFromDatabase(db))
      {
        Logger::info("Directory not found in database, creating: " + fullPath);
        dirMeta.storeToDatabase(db);
        DBUtils::addDirectoryToParent(db, fullPath);
      }
      // Remove the user prefix from fullPath to get the relative path.
      std::string relativePath = fullPath.substr(user.size() + 1);
      if (!FileUtils::createDirectory(relativePath, base_path))
      {
        Logger::error("Failed to create directory: " + relativePath);
      }
    }
    else if (event["item_type"] == "file")
    {
      // File update logic can be implemented here.
      Logger::info("File update event received (not yet implemented): " + event.dump());
    }
  }

  // Handle delete events for both files and directories.
  void handleDeleteEvent(const nlohmann::json &event, std::shared_ptr<rocksdb::DB> db)
  {
    std::string fullPath = event["full_path"].get<std::string>();
    // Try to treat as a file first.
    File_Metadata fileMeta;
    fileMeta.setFileName(fullPath);
    if (fileMeta.loadFromDatabase(db))
    {
      std::string relativePath = fullPath.substr(user.size() + 1);
      FileUtils::deleteFileSystemEntry(relativePath, base_path);
      DBUtils::deleteDBEntry(db, fullPath, "file");
      DBUtils::removeFileFromParentDirectory(db, fullPath);
      Logger::info("Deleted file from database: " + fullPath);
      return;
    }
    // Try to treat as a directory.
    std::string dirKey = fullPath;
    if (dirKey.back() != '/')
      dirKey += "/";
    Directory_Metadata dirMeta;
    dirMeta.setDirectoryName(dirKey);
    if (dirMeta.loadFromDatabase(db))
    {
      DBUtils::deleteDirectoryRecursively(db, dirKey);
      std::string relativePath = dirKey.substr(user.size() + 1);
      FileUtils::deleteFileSystemEntry(relativePath, base_path);
      DBUtils::removeDirectoryFromParent(db, dirKey);
      Logger::info("Deleted directory from database: " + dirKey);
      return;
    }
    Logger::warning("File or directory not found in database: " + fullPath);
  }

  // Process events from the server queue.
  void processServerEvents(std::shared_ptr<std::queue<nlohmann::json>> eventQueue,
                           std::shared_ptr<std::mutex> eventQueueMutex,
                           std::shared_ptr<rocksdb::DB> db)
  {
    while (true)
    {
      std::unique_lock<std::mutex> lock(*eventQueueMutex);
      if (eventQueue->empty())
      {
        break;
      }
      nlohmann::json event = eventQueue->front();
      eventQueue->pop();
      lock.unlock();

      Logger::info("Processing event: " + event.dump());
      std::string eventType = event["event_type"].get<std::string>();
      if (eventType == "update")
      {
        handleUpdateEvent(event, db);
      }
      else if (eventType == "delete")
      {
        handleDeleteEvent(event, db);
      }
      else
      {
        Logger::error("Unknown event type: " + event.dump());
      }
    }
  }

} // namespace EventHandler

//================== Main Function ==================
int main()
{
  // Load configuration
  nlohmann::json config;
  try
  {
    std::ifstream configFile(config_path);
    if (!configFile.is_open())
    {
      throw std::runtime_error("Failed to open config file: " + config_path);
    }
    config = nlohmann::json::parse(configFile);
  }
  catch (const nlohmann::json::parse_error &e)
  {
    throw std::runtime_error("JSON parsing error: " + std::string(e.what()));
  }

  if (!config.contains("access_token"))
    throw std::runtime_error("access_token not found in config file");

  if (!config.contains("metadata_database_path"))
    throw std::runtime_error("metadata_database_path not found in config file");

  user = config["user"].get<std::string>();
  base_path = config["monitoring_directory"].get<std::string>();

  // Open RocksDB
  std::string db_path = config["metadata_database_path"];
  rocksdb::Options options;
  options.create_if_missing = true;
  rocksdb::DB *raw_db = nullptr;
  rocksdb::Status status = rocksdb::DB::Open(options, db_path, &raw_db);
  if (!status.ok())
  {
    throw std::runtime_error("Failed to open RocksDB at path: " + db_path +
                             " Error: " + status.ToString());
  }
  std::shared_ptr<rocksdb::DB> db(raw_db, [](rocksdb::DB *ptr)
                                  { delete ptr; });

  // Bootup initialization (functions assumed to be defined elsewhere)
  bootup_1(config, db);
  std::string access_token = config["access_token"];
  std::shared_ptr<DropboxClient> dropboxClient = std::make_shared<DropboxClient>(access_token);
  bootup_2(db, dropboxClient, config);
  bootup_3(db, dropboxClient, config);

  // Setup event queue and mutex.
  auto eventQueue = std::make_shared<std::queue<nlohmann::json>>();
  auto eventQueueMutex = std::make_shared<std::mutex>();

  // Launch Dropbox long polling in a separate thread.
  std::thread longPollingThread([dropboxClient, &config, eventQueue, eventQueueMutex]()
                                { dropboxClient->monitorEvents(config["user"].get<std::string>(), eventQueue, eventQueueMutex); });

  // Main loop: process events as they come.
  while (true)
  {
    EventHandler::processServerEvents(eventQueue, eventQueueMutex, db);
    // Optionally sleep or yield here to avoid busy waiting.
  }

  longPollingThread.join();
  return 0;
}
