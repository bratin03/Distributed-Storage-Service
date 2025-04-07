#include "metadata.hpp"
#include "nlohmann/json.hpp"
#include "rocksdb/db.h"
#include <memory>
#include <set>
#include <string>
#include <vector>

using json = nlohmann::json;

namespace metadata
{

  // ---------- DBManager Implementation ----------

  std::shared_ptr<rocksdb::DB> DBManager::db_instance = nullptr;

  void DBManager::setDB(std::shared_ptr<rocksdb::DB> db)
  {
    if (!db_instance)
    {
      db_instance = db;
    }
  }

  std::shared_ptr<rocksdb::DB> DBManager::getDB()
  {
    return db_instance;
  }

  // ---------- File_Metadata Implementation ----------

  File_Metadata::File_Metadata()
      : fileName(""), fileSize(0), version(""), content_hash(""),
        file_content("") {}

  File_Metadata::File_Metadata(const std::string &name)
      : fileName(name), fileSize(0), version(""), content_hash(""),
        file_content("") {}

  File_Metadata::File_Metadata(const std::string &name, uint64_t size,
                               const std::string &ver, const std::string &hash,
                               const std::string &content)
      : fileName(name), fileSize(size), version(ver), content_hash(hash),
        file_content(content) {}

  void File_Metadata::setFileName(const std::string &name)
  {
    fileName = name;
  }

  std::string File_Metadata::getFileName() const { return fileName; }

  bool File_Metadata::storeToDatabase()
  {
    auto db = DBManager::getDB();
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
    auto db = DBManager::getDB();
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

  // ---------- Directory_Metadata Implementation ----------

  Directory_Metadata::Directory_Metadata()
      : files(), directories(), directoryName("") {}

  Directory_Metadata::Directory_Metadata(const std::string &name)
      : files(), directories(), directoryName(name) {}

  Directory_Metadata::Directory_Metadata(const std::string &name,
                                         const std::vector<std::string> &files,
                                         const std::vector<std::string> &directories)
      : files(files), directories(directories), directoryName(name) {}

  void Directory_Metadata::setDirectoryName(const std::string &name)
  {
    directoryName = name;
  }

  std::string Directory_Metadata::getDirectoryName() const
  {
    return directoryName;
  }

  bool Directory_Metadata::storeToDatabase()
  {
    auto db = DBManager::getDB();
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
    auto db = DBManager::getDB();
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

  // ---------- Prefix Scan Function ----------

  std::set<std::string> prefix_scan(const std::string &prefix)
  {
    std::set<std::string> result;
    auto db = DBManager::getDB();
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
        result.insert(key);
      }
    }
    return result;
  }

} // namespace metadata
