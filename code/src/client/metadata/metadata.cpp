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
} // namespace metadata
