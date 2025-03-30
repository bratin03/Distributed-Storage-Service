#include "dropbox/dropbox_client.h"
#include <fstream>
#include <nlohmann/json.hpp>
#include <string>
#include <thread>

const std::string config_path = "config.json";
std::string user;
std::string base_path;

namespace fs = std::filesystem;

// Helper to delete a key from the database with logging.
bool deleteKey(std::shared_ptr<rocksdb::DB> db,
               const std::string &key, const std::string &type)
{
  Logger::debug("Attempting to delete " + type + " from database: " + key);
  auto deleteStatus = db->Delete(rocksdb::WriteOptions(), key);
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

bool remove_file_from_parent_directory(std::shared_ptr<rocksdb::DB> db,
                                       const std::string &fileKey)
{
  // Find the last slash in the file key to get the parent directory.
  size_t lastSlash = fileKey.find_last_of('/');
  if (lastSlash == std::string::npos)
  {
    Logger::error("Invalid file key: " + fileKey);
    return false;
  }

  std::string parentDir = fileKey.substr(0, lastSlash);
  if (parentDir.back() != '/')
    parentDir += "/"; // Ensure it ends with a slash.
  Directory_Metadata parentDirMetadata;
  parentDirMetadata.setDirectoryName(parentDir);
  if (!parentDirMetadata.loadFromDatabase(db))
  {
    Logger::error("Parent directory not found in database: " + parentDir);
    return false;
  }
  // Remove the file from the parent directory's list of files.
  auto it = std::find(parentDirMetadata.files.begin(), parentDirMetadata.files.end(), fileKey);
  if (it != parentDirMetadata.files.end())
  {
    parentDirMetadata.files.erase(it);
    if (!parentDirMetadata.storeToDatabase(db))
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

void delete_file_fs(const std::string &name, const fs::path &base_path)
{
  // Construct the full path from the base path and the file/directory name.
  fs::path file_path = base_path / name;
  std::error_code ec;

  // Check if the path exists
  if (!fs::exists(file_path, ec))
  {
    Logger::error("Path does not exist: " + file_path.string());
    return;
  }

  // If it's a directory, delete it recursively; otherwise, delete the file.
  if (fs::is_directory(file_path, ec))
  {
    auto count = fs::remove_all(file_path, ec);
    if (ec)
    {
      Logger::error("Failed to delete directory: " + file_path.string() + " (" + ec.message() + ")");
    }
    else
    {
      Logger::info("Deleted directory: " + file_path.string() + " (" + std::to_string(count) + " files removed)");
    }
  }
  else
  {
    bool removed = fs::remove(file_path, ec);
    if (!removed || ec)
    {
      Logger::error("Failed to delete file: " + file_path.string() + " (" + ec.message() + ")");
    }
    else
    {
      Logger::info("Deleted file: " + file_path.string());
    }
  }
}

void processServerEvents(
    std::shared_ptr<std::queue<nlohmann::json>> eventQueueServer,
    std::shared_ptr<std::mutex> eventQueueMutex, std::shared_ptr<rocksdb::DB> db)
{
  while (true)
  {
    eventQueueMutex->lock();
    if (!eventQueueServer->empty())
    {
      nlohmann::json event = eventQueueServer->front();
      eventQueueServer->pop();
      eventQueueMutex->unlock();
      Logger::info("Processing event: " + event.dump());
      if (event["event_type"] == "update")
      {
      }
      else if (event["event_type"] == "delete")
      {
        std::string name = event["full_path"];
        // First check if it is a directory or file
        File_Metadata fileMetadata;
        fileMetadata.setFileName(name);
        if (fileMetadata.loadFromDatabase(db))
        {
          auto filepath = name.substr(user.size() + 1);
          delete_file_fs(filepath, base_path);
          deleteKey(db, name, "file");
          remove_file_from_parent_directory(db, name);
          Logger::info("Deleted file from database: " + name);
          continue;
        }
        name += "/";
        Directory_Metadata directoryMetadata;
        directoryMetadata.setDirectoryName(name);
        if (directoryMetadata.loadFromDatabase(db))
        {
          continue;
        }
        Logger::warning("File or directory not found in database: " + name);
      }
      else
      {
        Logger::error("Unknown event type: " + event.dump());
      }
    }
    else
    {
      eventQueueMutex->unlock();
      break;
    }
  }
}

int main()
{
  nlohmann::json config;
  try
  {
    std::ifstream config_file(config_path);
    if (!config_file.is_open())
    {
      throw std::runtime_error("Failed to open config file: " + config_path);
    }
    config = nlohmann::json::parse(config_file);
  }
  catch (const nlohmann::json::parse_error &e)
  {
    throw std::runtime_error("JSON parsing error: " + std::string(e.what()));
  }

  if (!config.contains("access_token"))
  {
    throw std::runtime_error("access_token not found in config file");
  }

  rocksdb::DB *raw_db = nullptr;
  if (!config.contains("metadata_database_path"))
  {
    throw std::runtime_error("metadata_database_path not found in config file");
  }

  user = config["user"].get<std::string>();
  base_path = config["monitoring_directory"].get<std::string>();

  std::string db_path = config["metadata_database_path"];
  rocksdb::Options options;
  options.create_if_missing = true;

  rocksdb::Status status = rocksdb::DB::Open(options, db_path, &raw_db);
  if (!status.ok())
  {
    throw std::runtime_error("Failed to open RocksDB at path: " + db_path +
                             " Error: " + status.ToString());
  }

  std::shared_ptr<rocksdb::DB> db(raw_db, [](rocksdb::DB *ptr)
                                  { delete ptr; });

  bootup_1(config, db);
  if (!config.contains("access_token"))
  {
    throw std::runtime_error("access_token not found in config file");
  }
  std::string access_token = config["access_token"];
  std::shared_ptr<DropboxClient> dropboxClient =
      std::make_shared<DropboxClient>(access_token);
  bootup_2(db, dropboxClient, config);
  bootup_3(db, dropboxClient, config);

  std::shared_ptr<std::queue<nlohmann::json>> eventQueueServer =
      std::make_shared<std::queue<nlohmann::json>>();
  std::shared_ptr<std::mutex> eventQueueMutex =
      std::make_shared<std::mutex>();

  std::thread longPollingThread([dropboxClient, &config, eventQueueServer, eventQueueMutex]()
                                { dropboxClient->monitorEvents(config["user"].get<std::string>(), eventQueueServer, eventQueueMutex); });

  for (;;)
  {
    processServerEvents(eventQueueServer, eventQueueMutex, db);
  }

  longPollingThread.join();
  return 0;
}
