#include "metadata.h"
#include "nlohmann/json.hpp"
#include "rocksdb/db.h"
#include <memory>
#include <set>
#include <string>
#include <vector>

using json = nlohmann::json;

// ---------- File_Metadata Implementation ----------

File_Metadata::File_Metadata()
    : fileName(""), fileSize(0), rev(""), latest_rev(""), content_hash(""),
      file_content("") {}

void File_Metadata::setFileName(const std::string &name) { fileName = name; }

std::string File_Metadata::getFileName() const { return fileName; }

bool File_Metadata::storeToDatabase(std::shared_ptr<rocksdb::DB> db) {
  json j;
  j["fileName"] = fileName;
  j["fileSize"] = fileSize;
  j["rev"] = rev;
  j["latest_rev"] = latest_rev;
  j["content_hash"] = content_hash;
  j["file_content"] = file_content;

  std::string key = fileName;
  std::string value = j.dump();

  rocksdb::Status status = db->Put(rocksdb::WriteOptions(), key, value);
  return status.ok();
}

bool File_Metadata::loadFromDatabase(std::shared_ptr<rocksdb::DB> db) {
  std::string key = fileName;
  std::string value;
  rocksdb::Status status = db->Get(rocksdb::ReadOptions(), key, &value);
  if (!status.ok()) {
    return false;
  }
  json j = json::parse(value);
  if (j.contains("fileName"))
    fileName = j["fileName"].get<std::string>();
  if (j.contains("fileSize"))
    fileSize = j["fileSize"].get<uint64_t>();
  if (j.contains("rev"))
    rev = j["rev"].get<std::string>();
  if (j.contains("latest_rev"))
    latest_rev = j["latest_rev"].get<std::string>();
  if (j.contains("content_hash"))
    content_hash = j["content_hash"].get<std::string>();
  if (j.contains("file_content"))
    file_content = j["file_content"].get<std::string>();
  return true;
}

// ---------- Directory_Metadata Implementation ----------

Directory_Metadata::Directory_Metadata()
    : directoryName(""), files(), directories() {}

void Directory_Metadata::setDirectoryName(const std::string &name) {
  directoryName = name;
}

std::string Directory_Metadata::getDirectoryName() const {
  return directoryName;
}

bool Directory_Metadata::storeToDatabase(std::shared_ptr<rocksdb::DB> db) {
  json j;
  j["directoryName"] = directoryName;
  j["files"] = files;
  j["directories"] = directories;

  std::string key = directoryName;
  std::string value = j.dump();

  rocksdb::Status status = db->Put(rocksdb::WriteOptions(), key, value);
  return status.ok();
}

bool Directory_Metadata::loadFromDatabase(std::shared_ptr<rocksdb::DB> db) {
  std::string key = directoryName;
  std::string value;
  rocksdb::Status status = db->Get(rocksdb::ReadOptions(), key, &value);
  if (!status.ok()) {
    return false;
  }
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

std::set<std::string> prefix_scan(std::shared_ptr<rocksdb::DB> db,
                                  const std::string &prefix) {
  std::set<std::string> result;
  std::unique_ptr<rocksdb::Iterator> it(
      db->NewIterator(rocksdb::ReadOptions()));

  for (it->Seek(prefix); it->Valid(); it->Next()) {
    std::string key = it->key().ToString();
    // If the key does not start with the given prefix, break out of the loop
    if (key.compare(0, prefix.size(), prefix) != 0) {
      break;
    }
    // Remove the prefix from the key
    std::string remainder = key.substr(prefix.size());
    // Skip if remainder is empty (can happen if the key equals the prefix
    // exactly)
    if (remainder.empty()) {
      continue;
    }
    // Only include the key if there is no additional '/' in the remainder.
    // This ensures we are not including keys from subdirectories.
    if (remainder.find('/') == std::string::npos) {
      result.insert(key);
    }
  }
  return result;
}
