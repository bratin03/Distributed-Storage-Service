/*
    CS60002 - Distributed Systems
    Term Project - Spring 2025

    * Author 1: Bratin Mondal (21CS10016)
    * Author 2: Soukhin Nayek (21CS10062)
    * Author 3: Swarnabh Mandal (21CS10068)

    * Department of Computer Science and Engineering
    * Indian Institute of Technology, Kharagpur
*/

#ifndef METADATA_HPP
#define METADATA_HPP

#include <memory>
#include <set>
#include <string>
#include <vector>

#include "Mylogger.hpp"
#include "nlohmann/json.hpp"
#include "rocksdb/db.h"

namespace metadata {

// Set the database instance for the namespace.
void setDatabase(std::shared_ptr<rocksdb::DB> db);

// Retrieve the database instance.
std::shared_ptr<rocksdb::DB> getDatabase();

// ----------------------------------
// File_Metadata Class Declaration
// ----------------------------------

class File_Metadata {
   public:
    File_Metadata();
    explicit File_Metadata(const std::string &name);
    File_Metadata(const std::string &name, uint64_t size, const std::string &version,
                  const std::string &content_hash, const std::string &file_content);

    bool storeToDatabase();
    bool loadFromDatabase();

    std::string _fileName;
    uint64_t _fileSize;
    std::string _version;
    std::string _contentHash;
    std::string _fileContent;
};

// -------------------------------------
// Directory_Metadata Class Declaration
// -------------------------------------

class Directory_Metadata {
   public:
    Directory_Metadata();
    explicit Directory_Metadata(const std::string &name);
    Directory_Metadata(const std::string &name, const std::vector<std::string> &files,
                       const std::vector<std::string> &directories);

    bool storeToDatabase();
    bool loadFromDatabase();

    std::vector<std::string> _files;
    std::vector<std::string> _directories;
    std::string _directoryName;
};

// -------------------------
// Prefix Scan Declaration
// -------------------------

std::set<std::string> prefix_scan(const std::string &prefix);

// -------------------------
// Remove File/Directory Declaration
// -------------------------
bool removeFileFromDatabase(const std::string &key);
bool removeDirectoryFromDatabase(const std::string &key);

// Add/Remove file/directory from parent directory
bool addFileToDirectory(const std::string &key);
bool removeFileFromDirectory(const std::string &key);

bool addDirectoryToDirectory(const std::string &key);
bool removeDirectoryFromDirectory(const std::string &key);

}  // namespace metadata

#endif  // METADATA_HPP
