#include "dropbox/dropbox_client.h"
#include <fstream>
#include <nlohmann/json.hpp>
#include <string>
#include <thread>
#include <filesystem>
#include <mutex>
#include <queue>
#include <set>
#include <algorithm>
#include <stdexcept>
#include <rocksdb/db.h>
#include "Watcher/recursive_inotify.hpp"

// Global variables (do not remove)
const std::string config_path = "config.json";
std::string user;
std::string base_path;
unsigned int sleep_time_ms;
unsigned int event_threshold;

namespace fs = std::filesystem;

//================== Dropbox Utilities ==================
namespace DropboxUtils
{

  std::string downloadFileContent(const std::string &filePath, std::shared_ptr<DropboxClient> dropboxClient)
  {
    auto response = dropboxClient->readFile(filePath);
    if (response.responseCode != 200)
      Logger::error("Failed to download file content for: " + filePath + ". Response: " + response.content);
    return (response.responseCode == 200) ? response.content : "";
  }

} // namespace DropboxUtils

//================== File System Utilities ==================
namespace FileSystemUtils
{

  bool createDirectoryIfNotExists(const std::string &relativePath, const fs::path &basePath)
  {
    fs::path fullPath = basePath / relativePath;
    std::error_code ec;
    if (!fs::create_directories(fullPath, ec))
    {
      if (ec)
      {
        Logger::error("Error creating directory: " + fullPath.string());
        return false;
      }
    }
    return true;
  }

  void deleteLocalEntry(const std::string &name, const fs::path &basePath)
  {
    fs::path fullPath = basePath / name;
    std::error_code ec;
    if (!fs::exists(fullPath, ec))
    {
      return;
    }
    if (fs::is_directory(fullPath, ec))
    {
      auto count = fs::remove_all(fullPath, ec);
      if (ec)
        Logger::error("Failed to remove directory: " + fullPath.string());
      else
        Logger::info("Removed directory (" + std::to_string(count) + " entries): " + fullPath.string());
    }
    else
    {
      bool removed = fs::remove(fullPath, ec);
      if (!removed || ec)
        Logger::error("Failed to remove file: " + fullPath.string());
      else
        Logger::info("Removed file: " + fullPath.string());
    }
  }

} // namespace FileSystemUtils

//================== Database Metadata Utilities ==================
namespace DatabaseUtils
{

  bool deleteDBEntry(std::shared_ptr<rocksdb::DB> db, const std::string &key, const std::string &entryType)
  {
    auto status = db->Delete(rocksdb::WriteOptions(), key);
    if (!status.ok())
    {
      Logger::error("Failed to delete " + entryType + " entry in DB: " + key);
      return false;
    }
    return true;
  }

  std::string getParentDirectoryKey(const std::string &key, bool isDirectory = false)
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

  bool registerFileInParentDirectory(std::shared_ptr<rocksdb::DB> db, const std::string &fileKey)
  {
    std::string parentDir = getParentDirectoryKey(fileKey);
    if (parentDir.empty())
    {
      return false;
    }
    Directory_Metadata parentMeta;
    parentMeta.setDirectoryName(parentDir);
    if (!parentMeta.loadFromDatabase(db))
      return false;
    if (std::find(parentMeta.files.begin(), parentMeta.files.end(), fileKey) == parentMeta.files.end())
    {
      parentMeta.files.push_back(fileKey);
      if (!parentMeta.storeToDatabase(db))
      {
        Logger::error("Failed to store updated metadata for parent directory: " + parentDir);
        return false;
      }
    }
    return true;
  }

  bool unregisterFileFromParentDirectory(std::shared_ptr<rocksdb::DB> db, const std::string &fileKey)
  {
    std::string parentDir = getParentDirectoryKey(fileKey);
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
      Logger::info("Unregistered file from parent directory: " + fileKey);
    }
    else
    {
      Logger::warning("File not found in parent directory's list: " + fileKey);
    }
    return true;
  }

  bool registerDirectoryInParent(std::shared_ptr<rocksdb::DB> db, const std::string &dirKey)
  {
    std::string parentDir = getParentDirectoryKey(dirKey, true);
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
      Logger::info("Registered directory in parent: " + dirKey);
    }
    else
    {
      Logger::warning("Directory already exists in parent directory's list: " + dirKey);
    }
    return true;
  }

  bool unregisterDirectoryFromParent(std::shared_ptr<rocksdb::DB> db, const std::string &dirKey)
  {
    std::string parentDir = getParentDirectoryKey(dirKey, true);
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
      Logger::info("Unregistered directory from parent: " + dirKey);
    }
    else
    {
      Logger::warning("Directory not found in parent directory's list: " + dirKey);
    }
    return true;
  }

  void deleteDirectoryRecursively(std::shared_ptr<rocksdb::DB> db, const std::string &dirKey)
  {
    Logger::debug("Deleting directory recursively in DB: " + dirKey);
    Directory_Metadata meta;
    meta.setDirectoryName(dirKey);
    if (!meta.loadFromDatabase(db))
    {
      Logger::error("Directory not found in database: " + dirKey);
      return;
    }
    deleteDBEntry(db, dirKey, "directory");
    for (const auto &fileKey : meta.files)
    {
      deleteDBEntry(db, fileKey, "file");
    }
    for (const auto &subDirKey : meta.directories)
    {
      deleteDirectoryRecursively(db, subDirKey);
    }
  }

} // namespace DatabaseUtils

//================== Event Processing Utilities ==================
namespace EventProcessor
{

  void handleFolderUpdate(const nlohmann::json &event, std::shared_ptr<rocksdb::DB> db)
  {
    std::string fullPath = event["full_path"].get<std::string>() + "/";
    Logger::debug("Handling folder update for: " + fullPath);
    Directory_Metadata dirMeta;
    dirMeta.setDirectoryName(fullPath);
    if (!dirMeta.loadFromDatabase(db))
    {
      Logger::info("Directory not in database. Creating new entry: " + fullPath);
      dirMeta.storeToDatabase(db);
      DatabaseUtils::registerDirectoryInParent(db, fullPath);
    }
    std::string relativePath = fullPath.substr(user.size() + 1);
    if (!FileSystemUtils::createDirectoryIfNotExists(relativePath, base_path))
      Logger::error("Failed to create local directory: " + relativePath);
    else
      Logger::info("Local directory created for folder update: " + relativePath);
  }

  void handleFileUpdate(const nlohmann::json &event, std::shared_ptr<rocksdb::DB> db,
                        std::shared_ptr<DropboxClient> dropboxClient)
  {
    Logger::debug("Handling file update event: " + event.dump());
    File_Metadata fileMeta;
    std::string filePath = event["full_path"].get<std::string>();
    fileMeta.setFileName(filePath);

    if (!fileMeta.loadFromDatabase(db))
    {
      Logger::info("File not found in DB: " + filePath);
      auto fileContent = DropboxUtils::downloadFileContent(filePath, dropboxClient);
      std::string relativePath = filePath.substr(user.size() + 1);
      FileSystemUtil::createFile(relativePath, base_path, fileContent);
      fileMeta.file_content = fileContent;
      fileMeta.content_hash = event["content_hash"].get<std::string>();
      fileMeta.fileSize = event["size"].get<int>();
      fileMeta.latest_rev = event["rev"].get<std::string>();
      fileMeta.rev = event["rev"].get<std::string>();
      fileMeta.storeToDatabase(db);
      DatabaseUtils::registerFileInParentDirectory(db, fileMeta.getFileName());
      Logger::info("Downloaded file content and stored metadata: " + fileMeta.getFileName());
    }
    else
    {
      std::string relativePath = filePath.substr(user.size() + 1);
      auto localContent = FileSystemUtil::readFileContent(relativePath, base_path);
      auto localHash = dropbox::compute_content_hash(localContent);
      std::string serverHash = event["content_hash"].get<std::string>();
      if (localHash != serverHash)
      {
        Logger::info("Hash mismatch for file: " + filePath);
        auto serverContent = DropboxUtils::downloadFileContent(filePath, dropboxClient);
        auto baseContent = fileMeta.file_content;
        std::string mergedContent;
        Logger::debug("Base content: " + baseContent);
        Logger::debug("Server content: " + serverContent);
        Logger::debug("Local content: " + localContent);
        bool mergeSuccessful = MergeLib::three_way_merge(baseContent, localContent, serverContent, mergedContent);
        if (mergeSuccessful)
        {
          Logger::info("Merge successful for file: " + filePath);
          Logger::debug("Merged content: " + mergedContent);
          fileMeta.file_content = mergedContent;
          FileSystemUtil::createFile(relativePath, base_path, mergedContent);
          auto response = dropboxClient->modifyFile(filePath, mergedContent, event["rev"].get<std::string>());
          if (response.responseCode == 200)
          {
            Logger::info("File updated on Dropbox: " + filePath);
            auto responseJson = nlohmann::json::parse(response.content);
            fileMeta.content_hash = responseJson["content_hash"].get<std::string>();
            fileMeta.fileSize = responseJson["size"].get<int>();
            fileMeta.rev = responseJson["rev"].get<std::string>();
            fileMeta.latest_rev = responseJson["rev"].get<std::string>();
            if (!fileMeta.storeToDatabase(db))
              Logger::error("Failed to update file metadata in DB: " + fileMeta.getFileName());
            else
              Logger::info("File metadata updated in DB: " + fileMeta.getFileName());
          }
          else
          {
            Logger::error("Failed to update file on Dropbox: " + response.content);
          }
        }
        else
        {
          Logger::error("Merge conflict for file: " + filePath);
          std::string conflictFileName;
          size_t dotPos = relativePath.find_last_of('.');
          size_t slashPos = relativePath.find_last_of('/');
          if (dotPos != std::string::npos && (slashPos == std::string::npos || dotPos > slashPos))
            conflictFileName = relativePath.substr(0, dotPos) + "$conflict$" + relativePath.substr(dotPos);
          else
            conflictFileName = relativePath + "$conflict$";
          // Save conflict file locally using the local version.
          FileSystemUtil::createFile(conflictFileName, base_path, localContent);
          Logger::error("Conflict file saved as: " + conflictFileName);
          // Update local file with merged or server content (based on your conflict resolution strategy).
          FileSystemUtil::createFile(relativePath, base_path, serverContent);
          std::string conflictFilePath = user + "/" + conflictFileName;
          fileMeta.file_content = serverContent;
          fileMeta.content_hash = event["content_hash"].get<std::string>();
          fileMeta.fileSize = event["size"].get<int>();
          fileMeta.latest_rev = event["rev"].get<std::string>();
          fileMeta.rev = event["rev"].get<std::string>();
          if (!fileMeta.storeToDatabase(db))
            Logger::error("Failed to update metadata in DB after conflict: " + fileMeta.getFileName());
          else
            Logger::info("File metadata updated after conflict: " + fileMeta.getFileName());

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
              Logger::error("Failed to store conflict file metadata in DB: " + conflictFilePath);
            else
              Logger::info("Conflict file metadata stored: " + conflictFilePath);
          }
          else
          {
            Logger::error("Failed to upload conflict file to Dropbox: " + conflictResponse.content);
          }
          DatabaseUtils::registerFileInParentDirectory(db, conflictFilePath);
        }
      }
      else
      {
        Logger::info("No changes for file: " + filePath + ". Updating metadata only.");
        fileMeta.content_hash = event["content_hash"].get<std::string>();
        fileMeta.fileSize = event["size"].get<int>();
        fileMeta.latest_rev = event["rev"].get<std::string>();
        fileMeta.rev = event["rev"].get<std::string>();
        if (!fileMeta.storeToDatabase(db))
          Logger::error("Failed to update metadata in DB for: " + fileMeta.getFileName());
        else
          Logger::info("File metadata updated: " + fileMeta.getFileName());
      }
    }
    Logger::info("Completed file update event: " + event.dump());
  }

  void handleDeleteEvent(const nlohmann::json &event, std::shared_ptr<rocksdb::DB> db)
  {
    std::string fullPath = event["full_path"].get<std::string>();
    Logger::debug("Handling delete event for: " + fullPath);
    File_Metadata fileMeta;
    fileMeta.setFileName(fullPath);
    if (fileMeta.loadFromDatabase(db))
    {
      std::string relativePath = fullPath.substr(user.size() + 1);
      FileSystemUtils::deleteLocalEntry(relativePath, base_path);
      DatabaseUtils::deleteDBEntry(db, fullPath, "file");
      DatabaseUtils::unregisterFileFromParentDirectory(db, fullPath);
      Logger::info("Deleted file from DB and local FS: " + fullPath);
      return;
    }
    std::string dirKey = fullPath;
    if (dirKey.back() != '/')
      dirKey += "/";
    Directory_Metadata dirMeta;
    dirMeta.setDirectoryName(dirKey);
    if (dirMeta.loadFromDatabase(db))
    {
      DatabaseUtils::deleteDirectoryRecursively(db, dirKey);
      std::string relativePath = dirKey.substr(user.size() + 1);
      FileSystemUtils::deleteLocalEntry(relativePath, base_path);
      DatabaseUtils::unregisterDirectoryFromParent(db, dirKey);
      Logger::info("Deleted directory from DB and local FS: " + dirKey);
      return;
    }
    Logger::warning("Neither file nor directory found in DB: " + fullPath);
  }

  void handleEvent(const nlohmann::json &event, std::shared_ptr<rocksdb::DB> db,
                   std::shared_ptr<DropboxClient> dropboxClient)
  {
    Logger::debug("Handling event: " + event.dump());
    std::string eventType = event["event_type"].get<std::string>();
    if (eventType == "update")
    {
      if (event["item_type"] == "folder")
        handleFolderUpdate(event, db);
      else if (event["item_type"] == "file")
        handleFileUpdate(event, db, dropboxClient);
      else
        Logger::error("Unknown update item_type: " + event.dump());
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

  void handleEventQueue(std::shared_ptr<std::queue<nlohmann::json>> eventQueue,
                        std::shared_ptr<std::mutex> eventQueueMutex,
                        std::shared_ptr<rocksdb::DB> db,
                        std::shared_ptr<DropboxClient> dropboxClient,
                        std::shared_ptr<std::mutex> databaseMutex)
  {
    while (true)
    {
      std::unique_lock<std::mutex> lock(*eventQueueMutex);
      if (eventQueue->empty())
      {
        lock.unlock();
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        continue;
      }
      nlohmann::json event = eventQueue->front();
      eventQueue->pop();
      lock.unlock();
      databaseMutex->lock();
      Logger::info("Processing event: " + event.dump());
      handleEvent(event, db, dropboxClient);
      databaseMutex->unlock();
    }
  }

} // namespace EventProcessor

//================== Local File Event Processing ==================
namespace LocalEventProcessor
{

  void handleLocalFileDeletion(std::shared_ptr<rocksdb::DB> db, std::shared_ptr<DropboxClient> dropboxClient,
                               const std::string &filePath)
  {
    Logger::info("Handling local file deletion: " + filePath);
    DatabaseUtils::deleteDBEntry(db, filePath, "file");
    DatabaseUtils::unregisterFileFromParentDirectory(db, filePath);
    auto response = dropboxClient->deleteFile(filePath);
    if (response.responseCode == 200)
      Logger::info("File deleted from Dropbox: " + filePath);
    else
      Logger::error("Failed to delete file from Dropbox: " + response.content);
  }

  void handleLocalFileCreation(std::shared_ptr<rocksdb::DB> db, std::shared_ptr<DropboxClient> dropboxClient,
                               const std::string &filePath, const std::string &fileFSPath)
  {
    Logger::info("Handling local file creation: " + filePath);
    File_Metadata fileMeta;
    fileMeta.setFileName(filePath);
    if (!fileMeta.loadFromDatabase(db))
    {
      fileMeta.file_content = FileSystemUtil::readFileContent(fileFSPath, base_path);
      auto response = dropboxClient->createFile(filePath, fileMeta.file_content);
      if (response.responseCode == 200)
      {
        Logger::info("File created on Dropbox: " + filePath);
        auto responseJson = nlohmann::json::parse(response.content);
        fileMeta.content_hash = responseJson["content_hash"].get<std::string>();
        fileMeta.fileSize = responseJson["size"].get<int>();
        fileMeta.rev = responseJson["rev"].get<std::string>();
        fileMeta.latest_rev = responseJson["rev"].get<std::string>();
        if (!fileMeta.storeToDatabase(db))
          Logger::error("Failed to store file metadata in DB: " + filePath);
        else
          Logger::info("File metadata stored: " + filePath);
        if (!DatabaseUtils::registerFileInParentDirectory(db, filePath))
          Logger::error("Failed to register file in parent directory: " + filePath);
        else
          Logger::info("File registered in parent directory: " + filePath);
      }
      else
      {
        Logger::error("Failed to create file on Dropbox: " + response.content);
      }
    }
    else
    {
      Logger::info("File already exists in DB: " + filePath);
    }
  }

  void handleLocalFileModification(std::shared_ptr<rocksdb::DB> db, std::shared_ptr<DropboxClient> dropboxClient,
                                   const std::string &filePath, const std::string &fileFSPath)
  {
    Logger::info("Handling local file modification: " + filePath);
    File_Metadata fileMeta;
    fileMeta.setFileName(filePath);
    if (fileMeta.loadFromDatabase(db))
    {
      auto localContent = FileSystemUtil::readFileContent(fileFSPath, base_path);
      auto response = dropboxClient->modifyFile(filePath, localContent, fileMeta.latest_rev);
      if (response.responseCode == 200)
      {
        Logger::info("File modified on Dropbox: " + filePath);
        auto responseJson = nlohmann::json::parse(response.content);
        fileMeta.content_hash = responseJson["content_hash"].get<std::string>();
        fileMeta.fileSize = responseJson["size"].get<int>();
        fileMeta.rev = responseJson["rev"].get<std::string>();
        fileMeta.latest_rev = responseJson["rev"].get<std::string>();
        if (!fileMeta.storeToDatabase(db))
          Logger::error("Failed to update metadata in DB: " + filePath);
        else
          Logger::info("File metadata updated: " + filePath);
      }
      else
      {
        Logger::error("Failed to modify file on Dropbox: " + response.content);
      }
    }
    else
    {
      Logger::info("File not found in DB: " + filePath);
    }
  }

  void handleLocalDirectoryDeletion(std::shared_ptr<rocksdb::DB> db, std::shared_ptr<DropboxClient> dropboxClient,
                                    const std::string &dirPath)
  {
    Logger::info("Handling local directory deletion: " + dirPath);
    DatabaseUtils::deleteDirectoryRecursively(db, dirPath);
    DatabaseUtils::unregisterDirectoryFromParent(db, dirPath);
    auto response = dropboxClient->deleteFolder(dirPath);
    if (response.responseCode == 200)
      Logger::info("Directory deleted from Dropbox: " + dirPath);
    else
      Logger::error("Failed to delete directory from Dropbox: " + response.content);
  }

  void handleLocalDirectoryCreation(std::shared_ptr<rocksdb::DB> db, std::shared_ptr<DropboxClient> dropboxClient,
                                    const std::string &dirPath)
  {
    Logger::info("Handling local directory creation: " + dirPath);
    Directory_Metadata dirMeta;
    dirMeta.setDirectoryName(dirPath);
    if (!dirMeta.loadFromDatabase(db))
    {
      dirMeta.storeToDatabase(db);
      auto response = dropboxClient->createFolder(dirPath);
      if (response.responseCode == 200)
      {
        Logger::info("Directory created on Dropbox: " + dirPath);
        if (!DatabaseUtils::registerDirectoryInParent(db, dirPath))
          Logger::error("Failed to register directory in parent: " + dirPath);
        else
          Logger::info("Directory registered in parent: " + dirPath);
      }
      else
      {
        Logger::error("Failed to create directory on Dropbox: " + response.content);
      }
    }
    else
    {
      Logger::info("Directory already exists in DB: " + dirPath);
    }
  }

  void processInotifyEvents(std::shared_ptr<std::queue<FileEvent>> inotifyEventQueue,
                            std::shared_ptr<std::set<FileEvent>> inotifyEventMap,
                            std::shared_ptr<std::mutex> inotifyEventMutex,
                            std::shared_ptr<std::condition_variable> inotifyEventConditionVariable,
                            std::shared_ptr<rocksdb::DB> db,
                            std::shared_ptr<DropboxClient> dropboxClient)
  {
    while (true)
    {
      std::unique_lock<std::mutex> lock(*inotifyEventMutex);
      auto wakeTime = std::chrono::steady_clock::now() + std::chrono::milliseconds(sleep_time_ms);
      inotifyEventConditionVariable->wait_until(lock, wakeTime, [&inotifyEventQueue]()
                                                { return inotifyEventQueue->size() >= event_threshold; });
      while (!inotifyEventQueue->empty())
      {
        auto event = inotifyEventQueue->front();
        inotifyEventQueue->pop();
        inotifyEventMap->erase(event);
        Logger::info("Processing inotify event: " + std::to_string(static_cast<int>(event.eventType)) +
                     " for path: " + event.path +
                     " of type: " + (event.fileType == FileType::File ? "File" : "Directory"));

        auto eventType = event.eventType;
        auto filePath = event.path;
        auto fileType = event.fileType;
        auto relativePath = filePath.substr(base_path.size() + 1);
        auto dropboxFilePath = user + "/" + relativePath;
        if (fileType == FileType::File)
        {
          if (eventType == InotifyEventType::Created || eventType == InotifyEventType::MovedTo)
          {
            handleLocalFileCreation(db, dropboxClient, dropboxFilePath, filePath);
          }
          else if (eventType == InotifyEventType::Modified)
          {
            handleLocalFileModification(db, dropboxClient, dropboxFilePath, filePath);
          }
          else if (eventType == InotifyEventType::Deleted || eventType == InotifyEventType::MovedFrom)
          {
            handleLocalFileDeletion(db, dropboxClient, dropboxFilePath);
          }
        }
        else if (fileType == FileType::Directory)
        {
          if (eventType == InotifyEventType::Created || eventType == InotifyEventType::MovedTo)
          {
            handleLocalDirectoryCreation(db, dropboxClient, dropboxFilePath);
          }
          else if (eventType == InotifyEventType::Deleted || eventType == InotifyEventType::MovedFrom)
          {
            handleLocalDirectoryDeletion(db, dropboxClient, dropboxFilePath);
          }
        }
        else
        {
          Logger::error("Unknown file type encountered: " + std::to_string(static_cast<int>(fileType)));
        }
      }
    }
  }

} // namespace LocalEventProcessor

//================== Initialization Functions ==================
namespace AppInitialization
{

  nlohmann::json loadConfigurationFromFile(const std::string &configPath)
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
    sleep_time_ms = config["sleep_time"].get<unsigned int>();
    event_threshold = config["event_threshold"].get<unsigned int>();
    Logger::info("User: " + user + ", Monitoring directory: " + base_path);
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
      throw std::runtime_error("Failed to open RocksDB at: " + db_path + " Error: " + status.ToString());
    Logger::info("RocksDB initialized successfully.");
    return std::shared_ptr<rocksdb::DB>(raw_db, [](rocksdb::DB *ptr)
                                        { delete ptr; });
  }

  std::shared_ptr<DropboxClient> initializeDropboxClient(const nlohmann::json &config)
  {
    std::string access_token = config["access_token"];
    Logger::info("Initializing DropboxClient.");
    return std::make_shared<DropboxClient>(access_token);
  }

  std::thread createLongPollingThread(std::shared_ptr<DropboxClient> dropboxClient,
                                      const nlohmann::json &config,
                                      std::shared_ptr<std::queue<nlohmann::json>> eventQueue,
                                      std::shared_ptr<std::mutex> eventQueueMutex)
  {
    Logger::info("Starting long polling thread.");
    return std::thread([dropboxClient, config, eventQueue, eventQueueMutex]()
                       {
    Logger::info("Long polling thread started for user: " + config["user"].get<std::string>());
    dropboxClient->monitorEvents(config["user"].get<std::string>(), eventQueue, eventQueueMutex); });
  }

} // namespace AppInitialization

//================== Main Function ==================
int main()
{
  Logger::info("Application starting...");

  // Load configuration and initialize services.
  nlohmann::json config = AppInitialization::loadConfigurationFromFile(config_path);
  auto db = AppInitialization::initializeDatabase(config);
  Logger::info("Running bootup_1...");
  bootup_1(config, db);
  Logger::info("Running bootup_2...");
  auto dropboxClient = AppInitialization::initializeDropboxClient(config);
  auto dropboxClientForComm_1 = AppInitialization::initializeDropboxClient(config);
  auto dropboxClientForComm_2 = AppInitialization::initializeDropboxClient(config);
  bootup_2(db, dropboxClient, config);
  Logger::info("Running bootup_3...");
  bootup_3(db, dropboxClient, config);

  // Setup event queues and mutexes.
  auto eventQueue = std::make_shared<std::queue<nlohmann::json>>();
  auto eventQueueMutex = std::make_shared<std::mutex>();
  auto dbMutex = std::make_shared<std::mutex>();
  auto inotifyEventQueue = std::make_shared<std::queue<FileEvent>>();
  auto inotifyEventMutex = std::make_shared<std::mutex>();
  auto inotifyEventCondVar = std::make_shared<std::condition_variable>();
  auto inotifyEventMap = std::make_shared<std::set<FileEvent>>();

  // Start threads.
  std::thread longPollingThread = AppInitialization::createLongPollingThread(dropboxClient, config, eventQueue, eventQueueMutex);
  std::thread eventServerProcessorThread(EventProcessor::handleEventQueue, eventQueue, eventQueueMutex, db, dropboxClientForComm_1, dbMutex);
  std::thread watcherThread(watch_directory, base_path, inotifyEventQueue, inotifyEventMap, inotifyEventMutex, inotifyEventCondVar);
  std::thread localEventProcessorThread(LocalEventProcessor::processInotifyEvents, inotifyEventQueue, inotifyEventMap, inotifyEventMutex, inotifyEventCondVar, db, dropboxClientForComm_2);

  // Join threads (they run indefinitely in this example).
  eventServerProcessorThread.join();
  longPollingThread.join();
  watcherThread.join();
  localEventProcessorThread.join();

  Logger::info("Application exiting.");
  return 0;
}
