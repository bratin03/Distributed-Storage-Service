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

// Global variables (do not remove)
const std::string config_path = "config.json";
std::string user;
std::string base_path;

namespace fs = std::filesystem;

//================== Dropbox Utilities ==================
std::string fetchDropboxFileContent(const std::string &filePath, std::shared_ptr<DropboxClient> dropboxClient)
{
  Logger::debug("Downloading file content for: " + filePath);
  auto response = dropboxClient->readFile(filePath);
  Logger::info("Download response for '" + filePath + "': " + std::to_string(response.responseCode));
  Logger::debug("Download content: " + response.content);
  if (response.responseCode != 200)
    Logger::error("Failed to download file content for: " + filePath + ". Response: " + response.content);
  return (response.responseCode == 200) ? response.content : "";
}

//================== File System Utilities ==================
namespace FileSystem
{

  bool createLocalDirectory(const std::string &relativePath, const fs::path &basePath)
  {
    fs::path fullPath = basePath / relativePath;
    Logger::debug("Attempting to create directory: " + fullPath.string());
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

  void removeFileSystemEntry(const std::string &name, const fs::path &basePath)
  {
    fs::path fullPath = basePath / name;
    Logger::debug("Attempting to remove filesystem entry: " + fullPath.string());
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

} // namespace FileSystem

//================== Database Metadata Utilities ==================
namespace Database
{

  bool removeDBEntry(std::shared_ptr<rocksdb::DB> db, const std::string &key, const std::string &entryType)
  {
    Logger::debug("Attempting to delete " + entryType + " from database: " + key);
    auto status = db->Delete(rocksdb::WriteOptions(), key);
    if (!status.ok())
    {
      Logger::error("Failed to delete " + entryType + " from database: " + key);
      return false;
    }
    Logger::info("Deleted " + entryType + " from database: " + key);
    return true;
  }

  std::string extractParentDirectory(const std::string &key, bool isDirectory = false)
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
    Logger::debug("Extracted parent directory: " + parentDir + " from key: " + key);
    return parentDir;
  }

  bool addFileToParentDirectory(std::shared_ptr<rocksdb::DB> db, const std::string &fileKey)
  {
    std::string parentDir = extractParentDirectory(fileKey);
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

  bool removeFileFromParentDirectory(std::shared_ptr<rocksdb::DB> db, const std::string &fileKey)
  {
    std::string parentDir = extractParentDirectory(fileKey);
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

  bool addDirectoryToParent(std::shared_ptr<rocksdb::DB> db, const std::string &dirKey)
  {
    std::string parentDir = extractParentDirectory(dirKey, true);
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

  bool removeDirectoryFromParent(std::shared_ptr<rocksdb::DB> db, const std::string &dirKey)
  {
    std::string parentDir = extractParentDirectory(dirKey, true);
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

  void deleteDirectoryRecursively(std::shared_ptr<rocksdb::DB> db, const std::string &dirKey)
  {
    Logger::debug("Deleting directory in DB recursively: " + dirKey);
    Directory_Metadata meta;
    meta.setDirectoryName(dirKey);
    if (!meta.loadFromDatabase(db))
    {
      Logger::error("Directory not found in database: " + dirKey);
      return;
    }
    removeDBEntry(db, dirKey, "directory");
    for (const auto &fileKey : meta.files)
    {
      removeDBEntry(db, fileKey, "file");
    }
    for (const auto &subDirKey : meta.directories)
    {
      deleteDirectoryRecursively(db, subDirKey);
    }
  }

} // namespace Database

//================== Event Processing Utilities ==================
namespace EventProcessor
{

  void processFolderUpdate(const nlohmann::json &event, std::shared_ptr<rocksdb::DB> db)
  {
    std::string fullPath = event["full_path"].get<std::string>() + "/";
    Logger::debug("Processing folder update for: " + fullPath);
    Directory_Metadata dirMeta;
    dirMeta.setDirectoryName(fullPath);
    if (!dirMeta.loadFromDatabase(db))
    {
      Logger::info("Directory not found in database, creating: " + fullPath);
      dirMeta.storeToDatabase(db);
      Database::addDirectoryToParent(db, fullPath);
    }
    std::string relativePath = fullPath.substr(user.size() + 1);
    if (!FileSystem::createLocalDirectory(relativePath, base_path))
    {
      Logger::error("Failed to create local directory: " + relativePath);
    }
    else
    {
      Logger::info("Local directory created for folder update: " + relativePath);
    }
  }

  void processFileUpdate(const nlohmann::json &event, std::shared_ptr<rocksdb::DB> db,
                         std::shared_ptr<DropboxClient> dropboxClient)
  {
    Logger::debug("Processing file update event: " + event.dump());
    File_Metadata fileMeta;
    std::string fileName = event["full_path"].get<std::string>();
    fileMeta.setFileName(fileName);

    if (!fileMeta.loadFromDatabase(db))
    {
      Logger::info("File not found in database: " + fileName);
      auto fileContent = fetchDropboxFileContent(fileName, dropboxClient);
      std::string relativePath = fileName.substr(user.size() + 1);
      FileSystemUtil::createFile(relativePath, base_path, fileContent);
      fileMeta.file_content = fileContent;
      fileMeta.content_hash = event["content_hash"].get<std::string>();
      fileMeta.fileSize = event["size"].get<int>();
      fileMeta.latest_rev = event["rev"].get<std::string>();
      fileMeta.rev = event["rev"].get<std::string>();
      fileMeta.storeToDatabase(db);
      Database::addFileToParentDirectory(db, fileMeta.getFileName());
      Logger::info("File content downloaded and metadata stored for: " + fileMeta.getFileName());
    }
    else
    {
      std::string relativePath = fileName.substr(user.size() + 1);
      auto localContent = FileSystemUtil::readFileContent(relativePath, base_path);
      auto localHash = dropbox::compute_content_hash(localContent);
      std::string serverHash = event["content_hash"].get<std::string>();
      if (localHash != serverHash)
      {
        Logger::info("Hash mismatch detected for file: " + fileName);
        auto serverContent = fetchDropboxFileContent(fileName, dropboxClient);
        auto baseContent = fileMeta.file_content;
        std::string finalContent;
        bool mergePossible = MergeLib::three_way_merge(baseContent, localContent, serverContent, finalContent);
        if (mergePossible)
        {
          Logger::info("Merge successful for file: " + fileName);
          fileMeta.file_content = finalContent;
          FileSystemUtil::createFile(relativePath, base_path, finalContent);
          auto response = dropboxClient->modifyFile(fileName, finalContent, event["rev"].get<std::string>());
          if (response.responseCode == 200)
          {
            Logger::info("File content updated successfully on Dropbox: " + fileName);
            auto responseJson = nlohmann::json::parse(response.content);
            fileMeta.content_hash = responseJson["content_hash"].get<std::string>();
            fileMeta.fileSize = responseJson["size"].get<int>();
            fileMeta.rev = responseJson["rev"].get<std::string>();
            fileMeta.latest_rev = responseJson["rev"].get<std::string>();
            if (!fileMeta.storeToDatabase(db))
            {
              Logger::error("Failed to update file metadata in database: " + fileMeta.getFileName());
            }
            else
            {
              Logger::info("File metadata updated successfully: " + fileMeta.getFileName());
            }
          }
          else
          {
            Logger::error("Failed to update file content on Dropbox: " + response.content);
          }
        }
        else
        {
          Logger::error("Merge conflict detected for file: " + fileName);
          std::string conflictFileName;
          size_t dotPos = relativePath.find_last_of('.');
          size_t slashPos = relativePath.find_last_of('/');
          if (dotPos != std::string::npos && (slashPos == std::string::npos || dotPos > slashPos))
            conflictFileName = relativePath.substr(0, dotPos) + "$conflict$" + relativePath.substr(dotPos);
          else
            conflictFileName = relativePath + "$conflict$";
          FileSystemUtil::createFile(conflictFileName, base_path, localContent);
          Logger::error("Merge conflict saved as: " + conflictFileName);
          FileSystemUtil::createFile(relativePath, base_path, finalContent);
          std::string conflictFilePath = user + "/" + conflictFileName;
          fileMeta.file_content = serverContent;
          fileMeta.content_hash = event["content_hash"].get<std::string>();
          fileMeta.fileSize = event["size"].get<int>();
          fileMeta.latest_rev = event["rev"].get<std::string>();
          fileMeta.rev = event["rev"].get<std::string>();
          if (!fileMeta.storeToDatabase(db))
            Logger::error("Failed to update file metadata in database: " + fileMeta.getFileName());
          else
            Logger::info("File metadata updated successfully after conflict: " + fileMeta.getFileName());

          File_Metadata conflictMeta;
          conflictMeta.setFileName(conflictFilePath);
          conflictMeta.content_hash = localHash;
          conflictMeta.fileSize = localContent.size();
          auto conflictResponse = dropboxClient->createFile(conflictFilePath, localContent);
          if (conflictResponse.responseCode == 200)
          {
            Logger::info("Conflict file uploaded to Dropbox: " + conflictFilePath);
            auto conflictResponseJson = nlohmann::json::parse(conflictResponse.content);
            conflictMeta.content_hash = conflictResponseJson["content_hash"].get<std::string>();
            conflictMeta.fileSize = conflictResponseJson["size"].get<int>();
            conflictMeta.rev = conflictResponseJson["rev"].get<std::string>();
            conflictMeta.latest_rev = conflictResponseJson["rev"].get<std::string>();
            if (!conflictMeta.storeToDatabase(db))
              Logger::error("Failed to store conflict file metadata in database: " + conflictFilePath);
            else
              Logger::info("Conflict file metadata stored successfully: " + conflictFilePath);
          }
          else
          {
            Logger::error("Failed to upload conflict file to Dropbox: " + conflictResponse.content);
          }
          Database::addFileToParentDirectory(db, conflictFilePath);
        }
      }
      else
      {
        Logger::info("No changes detected for file: " + fileName + ". Updating metadata.");
        fileMeta.content_hash = event["content_hash"].get<std::string>();
        fileMeta.fileSize = event["size"].get<int>();
        fileMeta.latest_rev = event["rev"].get<std::string>();
        fileMeta.rev = event["rev"].get<std::string>();
        if (!fileMeta.storeToDatabase(db))
          Logger::error("Failed to update file metadata in database: " + fileMeta.getFileName());
        else
          Logger::info("File metadata updated successfully: " + fileMeta.getFileName());
      }
    }
    Logger::info("File update event processed: " + event.dump());
  }

  void processDeleteEvent(const nlohmann::json &event, std::shared_ptr<rocksdb::DB> db)
  {
    std::string fullPath = event["full_path"].get<std::string>();
    Logger::debug("Processing delete event for: " + fullPath);
    File_Metadata fileMeta;
    fileMeta.setFileName(fullPath);
    if (fileMeta.loadFromDatabase(db))
    {
      std::string relativePath = fullPath.substr(user.size() + 1);
      FileSystem::removeFileSystemEntry(relativePath, base_path);
      Database::removeDBEntry(db, fullPath, "file");
      Database::removeFileFromParentDirectory(db, fullPath);
      Logger::info("Deleted file from database and local FS: " + fullPath);
      return;
    }
    std::string dirKey = fullPath;
    if (dirKey.back() != '/')
      dirKey += "/";
    Directory_Metadata dirMeta;
    dirMeta.setDirectoryName(dirKey);
    if (dirMeta.loadFromDatabase(db))
    {
      Database::deleteDirectoryRecursively(db, dirKey);
      std::string relativePath = dirKey.substr(user.size() + 1);
      FileSystem::removeFileSystemEntry(relativePath, base_path);
      Database::removeDirectoryFromParent(db, dirKey);
      Logger::info("Deleted directory from database and local FS: " + dirKey);
      return;
    }
    Logger::warning("File or directory not found in database: " + fullPath);
  }

  void processEvent(const nlohmann::json &event, std::shared_ptr<rocksdb::DB> db,
                    std::shared_ptr<DropboxClient> dropboxClient)
  {
    Logger::debug("Processing event: " + event.dump());
    std::string eventType = event["event_type"].get<std::string>();
    if (eventType == "update")
    {
      if (event["item_type"] == "folder")
        processFolderUpdate(event, db);
      else if (event["item_type"] == "file")
        processFileUpdate(event, db, dropboxClient);
      else
        Logger::error("Unknown update item_type: " + event.dump());
    }
    else if (eventType == "delete")
    {
      processDeleteEvent(event, db);
    }
    else
    {
      Logger::error("Unknown event type: " + event.dump());
    }
  }

  void processEventQueue(std::shared_ptr<std::queue<nlohmann::json>> eventQueue,
                         std::shared_ptr<std::mutex> eventQueueMutex,
                         std::shared_ptr<rocksdb::DB> db,
                         std::shared_ptr<DropboxClient> dropboxClient)
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
      processEvent(event, db, dropboxClient);
    }
  }

} // namespace EventProcessor

//================== Initialization Functions ==================
nlohmann::json loadConfiguration(const std::string &configPath)
{
  Logger::info("Loading configuration from: " + configPath);
  nlohmann::json config;
  std::ifstream configFile(configPath);
  if (!configFile.is_open())
    throw std::runtime_error("Failed to open config file: " + configPath);
  try
  {
    config = nlohmann::json::parse(configFile);
    Logger::debug("Configuration loaded: " + config.dump());
  }
  catch (const nlohmann::json::parse_error &e)
  {
    throw std::runtime_error("JSON parsing error: " + std::string(e.what()));
  }
  if (!config.contains("access_token"))
    throw std::runtime_error("access_token not found in config file");
  if (!config.contains("metadata_database_path"))
    throw std::runtime_error("metadata_database_path not found in config file");
  // Set globals
  user = config["user"].get<std::string>();
  base_path = config["monitoring_directory"].get<std::string>();
  Logger::info("User set to: " + user + " and monitoring directory: " + base_path);
  return config;
}

std::shared_ptr<rocksdb::DB> initializeDatabase(const nlohmann::json &config)
{
  std::string db_path = config["metadata_database_path"];
  Logger::info("Initializing RocksDB at: " + db_path);
  rocksdb::Options options;
  options.create_if_missing = true;
  rocksdb::DB *raw_db = nullptr;
  auto status = rocksdb::DB::Open(options, db_path, &raw_db);
  if (!status.ok())
    throw std::runtime_error("Failed to open RocksDB at path: " + db_path +
                             " Error: " + status.ToString());
  Logger::info("RocksDB initialized successfully.");
  return std::shared_ptr<rocksdb::DB>(raw_db, [](rocksdb::DB *ptr)
                                      { delete ptr; });
}

std::shared_ptr<DropboxClient> initializeDropboxClient(const nlohmann::json &config)
{
  std::string access_token = config["access_token"];
  Logger::info("Initializing DropboxClient with provided access token.");
  return std::make_shared<DropboxClient>(access_token);
}

std::shared_ptr<std::queue<nlohmann::json>> createEventQueue()
{
  Logger::info("Creating event queue.");
  return std::make_shared<std::queue<nlohmann::json>>();
}

std::shared_ptr<std::mutex> createEventQueueMutex()
{
  Logger::info("Creating event queue mutex.");
  return std::make_shared<std::mutex>();
}

std::thread startLongPollingThread(std::shared_ptr<DropboxClient> dropboxClient,
                                   const nlohmann::json &config,
                                   std::shared_ptr<std::queue<nlohmann::json>> eventQueue,
                                   std::shared_ptr<std::mutex> eventQueueMutex)
{
  Logger::info("Starting long polling thread.");
  return std::thread([dropboxClient, config, eventQueue, eventQueueMutex]()
                     { 
                       // Log before starting long polling.
                       Logger::info("Long polling thread started for user: " + config["user"].get<std::string>());
                       dropboxClient->monitorEvents(config["user"].get<std::string>(), eventQueue, eventQueueMutex); });
}

//================== Main Function ==================
int main()
{
  Logger::info("Application starting...");
  // Load configuration
  nlohmann::json config = loadConfiguration(config_path);

  // Initialize RocksDB
  auto db = initializeDatabase(config);

  // Bootup initialization (assumed to be defined elsewhere)
  Logger::info("Running bootup_1...");
  bootup_1(config, db);
  Logger::info("Running bootup_2...");
  auto dropboxClient = initializeDropboxClient(config);
  bootup_2(db, dropboxClient, config);
  Logger::info("Running bootup_3...");
  bootup_3(db, dropboxClient, config);

  // Setup event queue and mutex.
  auto eventQueue = createEventQueue();
  auto eventQueueMutex = createEventQueueMutex();

  // Start Dropbox long polling in a separate thread.
  std::thread longPollingThread = startLongPollingThread(dropboxClient, config, eventQueue, eventQueueMutex);

  // Main loop: process events as they arrive.
  Logger::info("Entering main event processing loop.");
  while (true)
  {
    EventProcessor::processEventQueue(eventQueue, eventQueueMutex, db, dropboxClient);
    std::this_thread::sleep_for(std::chrono::milliseconds(100)); // Avoid busy waiting.
  }

  longPollingThread.join();
  Logger::info("Application exiting.");
  return 0;
}
