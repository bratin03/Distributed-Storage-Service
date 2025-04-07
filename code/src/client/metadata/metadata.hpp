#ifndef METADATA_H
#define METADATA_H

#include "nlohmann/json.hpp"
#include "rocksdb/db.h"
#include <memory>
#include <set>
#include <string>
#include <vector>

namespace metadata
{

  class DBManager
  {
  public:
    static void setDB(std::shared_ptr<rocksdb::DB> db);
    static std::shared_ptr<rocksdb::DB> getDB();

  private:
    static std::shared_ptr<rocksdb::DB> db_instance;
  };

  class File_Metadata
  {
  public:
    File_Metadata();
    File_Metadata(const std::string &name);
    File_Metadata(const std::string &name, uint64_t size,
                  const std::string &ver, const std::string &hash,
                  const std::string &content);

    void setFileName(const std::string &name);
    std::string getFileName() const;

    bool storeToDatabase();
    bool loadFromDatabase();

    std::string fileName;
    uint64_t fileSize;
    std::string version;
    std::string content_hash;
    std::string file_content;
  };

  class Directory_Metadata
  {
  public:
    Directory_Metadata();
    Directory_Metadata(const std::string &name);
    Directory_Metadata(const std::string &name,
                        const std::vector<std::string> &files,
                        const std::vector<std::string> &directories);

    void setDirectoryName(const std::string &name);
    std::string getDirectoryName() const;

    bool storeToDatabase();
    bool loadFromDatabase();

    std::vector<std::string> files;
    std::vector<std::string> directories;
    std::string directoryName;
  };

  std::set<std::string> prefix_scan(const std::string &prefix);

} // namespace metadata

#endif // METADATA_H
