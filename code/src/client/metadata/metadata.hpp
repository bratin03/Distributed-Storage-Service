#ifndef METADATA_HPP
#define METADATA_HPP

#include <string>
#include <vector>
#include <set>
#include <memory>
#include "nlohmann/json.hpp"
#include "rocksdb/db.h"
#include "../logger/Mylogger.hpp"

namespace metadata
{

  // Set the database instance for the namespace.
  void setDatabase(std::shared_ptr<rocksdb::DB> db);

  // Retrieve the database instance.
  std::shared_ptr<rocksdb::DB> getDatabase();

  // ----------------------------------
  // File_Metadata Class Declaration
  // ----------------------------------

  class File_Metadata
  {
  public:
    File_Metadata();
    explicit File_Metadata(const std::string &name);
    File_Metadata(const std::string &name, uint64_t size,
                  const std::string &version, const std::string &content_hash,
                  const std::string &file_content);

    bool storeToDatabase();
    bool loadFromDatabase();

    std::string fileName;
    uint64_t fileSize;
    std::string version;
    std::string content_hash;
    std::string file_content;
  };

  // -------------------------------------
  // Directory_Metadata Class Declaration
  // -------------------------------------

  class Directory_Metadata
  {
  public:
    Directory_Metadata();
    explicit Directory_Metadata(const std::string &name);
    Directory_Metadata(const std::string &name,
                       const std::vector<std::string> &files,
                       const std::vector<std::string> &directories);

    bool storeToDatabase();
    bool loadFromDatabase();

    std::vector<std::string> files;
    std::vector<std::string> directories;
    std::string directoryName;
  };

  // -------------------------
  // Prefix Scan Declaration
  // -------------------------

  std::set<std::string> prefix_scan(const std::string &prefix);
  bool removeFileFromDatabase(const std::string &key);
  bool removeDirectoryFromDatabase(const std::string &key);

} // namespace metadata

#endif // METADATA_HPP
