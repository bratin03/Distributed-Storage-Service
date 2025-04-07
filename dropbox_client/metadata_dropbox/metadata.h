#ifndef METADATA_H
#define METADATA_H

#include "nlohmann/json.hpp"
#include "rocksdb/db.h"
#include <memory>
#include <set>
#include <string>
#include <vector>

class File_Metadata
{
public:
  File_Metadata();

  void setFileName(const std::string &name);
  std::string getFileName() const;

  // Database methods:
  bool storeToDatabase(std::shared_ptr<rocksdb::DB> db);
  bool loadFromDatabase(std::shared_ptr<rocksdb::DB> db);

  std::string fileName;
  uint64_t fileSize;
  std::string rev;
  std::string latest_rev;
  std::string content_hash;
  std::string file_content;
};

class Directory_Metadata
{
public:
  Directory_Metadata();

  void setDirectoryName(const std::string &name);
  std::string getDirectoryName() const;

  // Database methods:
  bool storeToDatabase(std::shared_ptr<rocksdb::DB> db);
  bool loadFromDatabase(std::shared_ptr<rocksdb::DB> db);
  std::vector<std::string> files;
  std::vector<std::string> directories;

  std::string directoryName;
};

// Prefix scan function that returns all keys starting with the given prefix.
std::set<std::string> prefix_scan(std::shared_ptr<rocksdb::DB> db,
                                  const std::string &prefix);

#endif // METADATA_H
