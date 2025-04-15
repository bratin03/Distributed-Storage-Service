#include "metadata.hpp"

using json = nlohmann::json;

namespace metadata
{

  // Static instance to hold the database pointer.
  static std::shared_ptr<rocksdb::DB> db_instance = nullptr;

  // Sets the database pointer.
  // Only sets it if it hasn't been set previously.
  void setDatabase(std::shared_ptr<rocksdb::DB> db)
  {
    if (!db_instance)
    {
      db_instance = db;
    }
  }

  // Retrieves the current database pointer.
  std::shared_ptr<rocksdb::DB> getDatabase()
  {
    return db_instance;
  }

  // ------------------------------
  // File_Metadata Implementation
  // ------------------------------

  File_Metadata::File_Metadata()
      : fileName(""), fileSize(0), version("0"), content_hash(""), file_content("") {}

  File_Metadata::File_Metadata(const std::string &name)
      : fileName(name), fileSize(0), version("0"), content_hash(""), file_content("") {}

  File_Metadata::File_Metadata(const std::string &name, uint64_t size,
                               const std::string &ver, const std::string &hash,
                               const std::string &content)
      : fileName(name), fileSize(size), version(ver), content_hash(hash), file_content(content) {}

  bool File_Metadata::storeToDatabase()
  {
    auto db = getDatabase();
    if (!db)
      return false;

    json j;
    j["fileName"] = fileName;
    j["fileSize"] = fileSize;
    j["version"] = version;
    j["content_hash"] = content_hash;
    j["file_content"] = file_content;

    std::string key = fileName;
    std::string value = j.dump();

    rocksdb::Status status = db->Put(rocksdb::WriteOptions(), key, value);
    return status.ok();
  }

  bool File_Metadata::loadFromDatabase()
  {
    auto db = getDatabase();
    if (!db)
      return false;

    std::string key = fileName;
    std::string value;
    rocksdb::Status status = db->Get(rocksdb::ReadOptions(), key, &value);
    if (!status.ok())
      return false;

    json j = json::parse(value);
    if (j.contains("fileName"))
      fileName = j["fileName"].get<std::string>();
    if (j.contains("fileSize"))
      fileSize = j["fileSize"].get<uint64_t>();
    if (j.contains("version"))
      version = j["version"].get<std::string>();
    if (j.contains("content_hash"))
      content_hash = j["content_hash"].get<std::string>();
    if (j.contains("file_content"))
      file_content = j["file_content"].get<std::string>();

    return true;
  }

  // --------------------------------------
  // Directory_Metadata Implementation
  // --------------------------------------

  Directory_Metadata::Directory_Metadata()
      : files(), directories(), directoryName("") {}

  Directory_Metadata::Directory_Metadata(const std::string &name)
      : files(), directories(), directoryName(name) {}

  Directory_Metadata::Directory_Metadata(const std::string &name,
                                         const std::vector<std::string> &files,
                                         const std::vector<std::string> &directories)
      : files(files), directories(directories), directoryName(name) {}

  bool Directory_Metadata::storeToDatabase()
  {
    auto db = getDatabase();
    if (!db)
      return false;

    json j;
    j["directoryName"] = directoryName;
    j["files"] = files;
    j["directories"] = directories;

    std::string key = directoryName;
    std::string value = j.dump();

    rocksdb::Status status = db->Put(rocksdb::WriteOptions(), key, value);
    return status.ok();
  }

  bool Directory_Metadata::loadFromDatabase()
  {
    auto db = getDatabase();
    if (!db)
      return false;

    std::string key = directoryName;
    std::string value;
    rocksdb::Status status = db->Get(rocksdb::ReadOptions(), key, &value);
    if (!status.ok())
      return false;

    json j = json::parse(value);
    if (j.contains("directoryName"))
      directoryName = j["directoryName"].get<std::string>();
    if (j.contains("files"))
      files = j["files"].get<std::vector<std::string>>();
    if (j.contains("directories"))
      directories = j["directories"].get<std::vector<std::string>>();

    return true;
  }

  // -----------------------------
  // Prefix Scan Implementation
  // -----------------------------

  std::set<std::string> prefix_scan(const std::string &prefix)
  {
    MyLogger::debug("Scanning for prefix: " + prefix);
    std::set<std::string> result;
    auto db = getDatabase();
    if (!db)
      return result;

    std::unique_ptr<rocksdb::Iterator> it(db->NewIterator(rocksdb::ReadOptions()));

    for (it->Seek(prefix); it->Valid(); it->Next())
    {
      std::string key = it->key().ToString();
      if (key.compare(0, prefix.size(), prefix) != 0)
        break;

      std::string remainder = key.substr(prefix.size());
      if (remainder.empty())
        continue;
      if (remainder.find('/') == std::string::npos)
      {
        MyLogger::debug("Found file key: " + key);
        result.insert(key);
      }
    }
    return result;
  }

  bool removeFileFromDatabase(const std::string &key)
  {
    auto db = getDatabase();
    if (!db)
      return false;

    rocksdb::Status status = db->Delete(rocksdb::WriteOptions(), key);
    MyLogger::warning("Removing file from database: " + key);
    return status.ok();
  }

  bool removeDirectoryFromDatabase(const std::string &key)
  {
    MyLogger::warning("Removing directory from database: " + key);
    auto db = getDatabase();
    if (!db)
      return false;
    // First, read the directory metadata
    metadata::Directory_Metadata dir_metadata(key);
    if (!dir_metadata.loadFromDatabase())
    {
      return false;
    }
    auto files = dir_metadata.files;
    auto directories = dir_metadata.directories;
    // Remove all files in the directory
    for (const auto &file : files)
    {
      removeFileFromDatabase(file);
    }
    // Remove all subdirectories
    for (const auto &dir : directories)
    {
      removeDirectoryFromDatabase(dir);
    }
    // Finally, remove the directory itself
    rocksdb::Status status = db->Delete(rocksdb::WriteOptions(), key);
    return status.ok();
  }

  // -----------------------------
  // File/Directory Removal Implementation
  // -----------------------------

  bool addFileToDirectory(const std::string &key)
  {
    // Parse the key to get the directory name
    size_t lastSlash = key.find_last_of('/');
    if (lastSlash == std::string::npos)
    {
      MyLogger::error("Invalid file key: " + key);
      return false;
    }
    std::string dir_key = key.substr(0, lastSlash);
    Directory_Metadata dir_metadata(dir_key);
    if (!dir_metadata.loadFromDatabase())
    {
      MyLogger::error("Failed to load directory metadata from database for: " + dir_key);
      return false;
    }
    // Add the file to the directory's list of files if it doesn't already exist
    if (std::find(dir_metadata.files.begin(), dir_metadata.files.end(), key) == dir_metadata.files.end())
    {
      dir_metadata.files.push_back(key);
      if (!dir_metadata.storeToDatabase())
      {
        MyLogger::error("Failed to update directory metadata in database for: " + dir_key);
        return false;
      }
    }
    else
    {
      MyLogger::info("File already exists in directory: " + key);
    }
    return true;
  }

  bool removeFileFromDirectory(const std::string &key)
  {
    // Parse the key to get the directory name
    size_t lastSlash = key.find_last_of('/');
    if (lastSlash == std::string::npos)
    {
      MyLogger::error("Invalid file key: " + key);
      return false;
    }
    std::string dir_key = key.substr(0, lastSlash);
    Directory_Metadata dir_metadata(dir_key);
    if (!dir_metadata.loadFromDatabase())
    {
      MyLogger::error("Failed to load directory metadata from database for: " + dir_key);
      return false;
    }
    // Remove the file from the directory's list of files
    auto it = std::remove(dir_metadata.files.begin(), dir_metadata.files.end(), key);
    if (it != dir_metadata.files.end())
    {
      dir_metadata.files.erase(it, dir_metadata.files.end());
      if (!dir_metadata.storeToDatabase())
      {
        MyLogger::error("Failed to update directory metadata in database for: " + dir_key);
        return false;
      }
    }
    else
    {
      MyLogger::info("File not found in directory: " + key);
    }
    return true;
  }

  bool addDirectoryToDirectory(const std::string &key)
  {
    // Parse the key to get the parent directory name
    size_t lastSlash = key.find_last_of('/');
    if (lastSlash == std::string::npos)
    {
      MyLogger::error("Invalid directory key: " + key);
      return false;
    }
    std::string parent_dir_key = key.substr(0, lastSlash);
    Directory_Metadata dir_metadata(parent_dir_key);
    if (!dir_metadata.loadFromDatabase())
    {
      MyLogger::error("Failed to load directory metadata from database for: " + parent_dir_key);
      return false;
    }
    // Add the directory to the parent's list of directories if it doesn't already exist
    if (std::find(dir_metadata.directories.begin(), dir_metadata.directories.end(), key) == dir_metadata.directories.end())
    {
      dir_metadata.directories.push_back(key);
      if (!dir_metadata.storeToDatabase())
      {
        MyLogger::error("Failed to update directory metadata in database for: " + parent_dir_key);
        return false;
      }
    }
    else
    {
      MyLogger::info("Directory already exists in parent directory: " + key);
    }
    return true;
  }

  bool removeDirectoryFromDirectory(const std::string &key)
  {
    // Parse the key to get the parent directory name
    size_t lastSlash = key.find_last_of('/');
    if (lastSlash == std::string::npos)
    {
      MyLogger::error("Invalid directory key: " + key);
      return false;
    }
    std::string parent_dir_key = key.substr(0, lastSlash);
    Directory_Metadata dir_metadata(parent_dir_key);
    if (!dir_metadata.loadFromDatabase())
    {
      MyLogger::error("Failed to load directory metadata from database for: " + parent_dir_key);
      return false;
    }
    // Remove the directory from the parent's list of directories
    auto it = std::remove(dir_metadata.directories.begin(), dir_metadata.directories.end(), key);
    if (it != dir_metadata.directories.end())
    {
      dir_metadata.directories.erase(it, dir_metadata.directories.end());
      if (!dir_metadata.storeToDatabase())
      {
        MyLogger::error("Failed to update directory metadata in database for: " + parent_dir_key);
        return false;
      }
    }
    else
    {
      MyLogger::info("Directory not found in parent directory: " + key);
    }
    return true;
  }
} // namespace metadata
