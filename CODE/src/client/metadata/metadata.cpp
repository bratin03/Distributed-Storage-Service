/*
    CS60002 - Distributed Systems
    Term Project - Spring 2025

    * Author 1: Bratin Mondal (21CS10016)
    * Author 2: Soukhin Nayek (21CS10062)
    * Author 3: Swarnabh Mandal (21CS10068)

    * Department of Computer Science and Engineering
    * Indian Institute of Technology, Kharagpur
*/

#include "metadata.hpp"

using json = nlohmann::json;

namespace metadata {

// Static instance to hold the database pointer.
static std::shared_ptr<rocksdb::DB> db_instance = nullptr;

// Sets the database pointer.
// Only sets it if it hasn't been set previously.
void setDatabase(std::shared_ptr<rocksdb::DB> db) {
    if (!db_instance) {
        db_instance = db;
    }
}

// Retrieves the current database pointer.
std::shared_ptr<rocksdb::DB> getDatabase() {
    return db_instance;
}

// ------------------------------
// File_Metadata Implementation
// ------------------------------

File_Metadata::File_Metadata()
    : _fileName(""), _fileSize(0), _version("0"), _contentHash(""), _fileContent("") {}

File_Metadata::File_Metadata(const std::string &name)
    : _fileName(name), _fileSize(0), _version("0"), _contentHash(""), _fileContent("") {}

File_Metadata::File_Metadata(const std::string &name, uint64_t size, const std::string &ver,
                             const std::string &hash, const std::string &content)
    : _fileName(name), _fileSize(size), _version(ver), _contentHash(hash), _fileContent(content) {}

bool File_Metadata::storeToDatabase() {
    auto db = getDatabase();
    if (!db) return false;

    json j;
    j["fileName"] = _fileName;
    j["fileSize"] = _fileSize;
    j["version"] = _version;
    j["content_hash"] = _contentHash;
    j["file_content"] = _fileContent;

    std::string key = _fileName;
    std::string value = j.dump();

    rocksdb::Status status = db->Put(rocksdb::WriteOptions(), key, value);
    return status.ok();
}

bool File_Metadata::loadFromDatabase() {
    auto db = getDatabase();
    if (!db) return false;

    std::string key = _fileName;
    std::string value;
    rocksdb::Status status = db->Get(rocksdb::ReadOptions(), key, &value);
    if (!status.ok()) return false;

    json j = json::parse(value);
    if (j.contains("fileName")) _fileName = j["fileName"].get<std::string>();
    if (j.contains("fileSize")) _fileSize = j["fileSize"].get<uint64_t>();
    if (j.contains("version")) _version = j["version"].get<std::string>();
    if (j.contains("content_hash")) _contentHash = j["content_hash"].get<std::string>();
    if (j.contains("file_content")) _fileContent = j["file_content"].get<std::string>();

    return true;
}

// --------------------------------------
// Directory_Metadata Implementation
// --------------------------------------

Directory_Metadata::Directory_Metadata() : _files(), _directories(), _directoryName("") {}

Directory_Metadata::Directory_Metadata(const std::string &name)
    : _files(), _directories(), _directoryName(name) {}

Directory_Metadata::Directory_Metadata(const std::string &name,
                                       const std::vector<std::string> &files,
                                       const std::vector<std::string> &directories)
    : _files(files), _directories(directories), _directoryName(name) {}

bool Directory_Metadata::storeToDatabase() {
    auto db = getDatabase();
    if (!db) return false;

    json j;
    j["directoryName"] = _directoryName;
    j["files"] = _files;
    j["directories"] = _directories;

    std::string key = _directoryName;
    std::string value = j.dump();

    rocksdb::Status status = db->Put(rocksdb::WriteOptions(), key, value);
    return status.ok();
}

bool Directory_Metadata::loadFromDatabase() {
    auto db = getDatabase();
    if (!db) return false;

    std::string key = _directoryName;
    std::string value;
    rocksdb::Status status = db->Get(rocksdb::ReadOptions(), key, &value);
    if (!status.ok()) return false;

    json j = json::parse(value);
    if (j.contains("directoryName")) _directoryName = j["directoryName"].get<std::string>();
    if (j.contains("files")) _files = j["files"].get<std::vector<std::string>>();
    if (j.contains("directories")) _directories = j["directories"].get<std::vector<std::string>>();

    return true;
}

// -----------------------------
// Prefix Scan Implementation
// -----------------------------

std::set<std::string> prefix_scan(const std::string &prefix) {
    MyLogger::debug("Scanning for prefix: " + prefix);
    std::set<std::string> result;
    auto db = getDatabase();
    if (!db) return result;

    std::unique_ptr<rocksdb::Iterator> it(db->NewIterator(rocksdb::ReadOptions()));

    for (it->Seek(prefix); it->Valid(); it->Next()) {
        std::string key = it->key().ToString();
        if (key.compare(0, prefix.size(), prefix) != 0) break;

        std::string remainder = key.substr(prefix.size());
        if (remainder.empty()) continue;
        if (remainder.find('/') == std::string::npos) {
            MyLogger::debug("Found file key: " + key);
            result.insert(key);
        }
    }
    return result;
}

bool removeFileFromDatabase(const std::string &key) {
    auto db = getDatabase();
    if (!db) return false;

    rocksdb::Status status = db->Delete(rocksdb::WriteOptions(), key);
    MyLogger::warning("Removing file from database: " + key);
    return status.ok();
}

bool removeDirectoryFromDatabase(const std::string &key) {
    MyLogger::warning("Removing directory from database: " + key);
    auto db = getDatabase();
    if (!db) return false;
    // First, read the directory metadata
    metadata::Directory_Metadata dir_metadata(key);
    if (!dir_metadata.loadFromDatabase()) {
        return false;
    }
    auto files = dir_metadata._files;
    auto directories = dir_metadata._directories;
    // Remove all files in the directory
    for (const auto &file : files) {
        removeFileFromDatabase(file);
    }
    // Remove all subdirectories
    for (const auto &dir : directories) {
        removeDirectoryFromDatabase(dir);
    }
    // Finally, remove the directory itself
    rocksdb::Status status = db->Delete(rocksdb::WriteOptions(), key);
    return status.ok();
}

// -----------------------------
// File/Directory Removal Implementation
// -----------------------------

bool addFileToDirectory(const std::string &key) {
    // Parse the key to get the directory name
    size_t lastSlash = key.find_last_of('/');
    if (lastSlash == std::string::npos) {
        MyLogger::error("Invalid file key: " + key);
        return false;
    }
    std::string dir_key = key.substr(0, lastSlash);
    Directory_Metadata dir_metadata(dir_key);
    if (!dir_metadata.loadFromDatabase()) {
        MyLogger::error("Failed to load directory metadata from database for: " + dir_key);
        return false;
    }
    // Add the file to the directory's list of files if it doesn't already exist
    if (std::find(dir_metadata._files.begin(), dir_metadata._files.end(), key) ==
        dir_metadata._files.end()) {
        dir_metadata._files.push_back(key);
        if (!dir_metadata.storeToDatabase()) {
            MyLogger::error("Failed to update directory metadata in database for: " + dir_key);
            return false;
        }
    } else {
        MyLogger::info("File already exists in directory: " + key);
    }
    return true;
}

bool removeFileFromDirectory(const std::string &key) {
    // Parse the key to get the directory name
    size_t lastSlash = key.find_last_of('/');
    if (lastSlash == std::string::npos) {
        MyLogger::error("Invalid file key: " + key);
        return false;
    }
    std::string dir_key = key.substr(0, lastSlash);
    Directory_Metadata dir_metadata(dir_key);
    if (!dir_metadata.loadFromDatabase()) {
        MyLogger::error("Failed to load directory metadata from database for: " + dir_key);
        return false;
    }
    // Remove the file from the directory's list of files
    auto it = std::remove(dir_metadata._files.begin(), dir_metadata._files.end(), key);
    if (it != dir_metadata._files.end()) {
        dir_metadata._files.erase(it, dir_metadata._files.end());
        if (!dir_metadata.storeToDatabase()) {
            MyLogger::error("Failed to update directory metadata in database for: " + dir_key);
            return false;
        }
    } else {
        MyLogger::info("File not found in directory: " + key);
    }
    return true;
}

bool addDirectoryToDirectory(const std::string &key) {
    // Parse the key to get the parent directory name
    size_t lastSlash = key.find_last_of('/');
    if (lastSlash == std::string::npos) {
        MyLogger::error("Invalid directory key: " + key);
        return false;
    }
    std::string parent_dir_key = key.substr(0, lastSlash);
    Directory_Metadata dir_metadata(parent_dir_key);
    if (!dir_metadata.loadFromDatabase()) {
        MyLogger::error("Failed to load directory metadata from database for: " + parent_dir_key);
        return false;
    }
    // Add the directory to the parent's list of directories if it doesn't already exist
    if (std::find(dir_metadata._directories.begin(), dir_metadata._directories.end(), key) ==
        dir_metadata._directories.end()) {
        dir_metadata._directories.push_back(key);
        if (!dir_metadata.storeToDatabase()) {
            MyLogger::error("Failed to update directory metadata in database for: " +
                            parent_dir_key);
            return false;
        }
    } else {
        MyLogger::info("Directory already exists in parent directory: " + key);
    }
    return true;
}

bool removeDirectoryFromDirectory(const std::string &key) {
    // Parse the key to get the parent directory name
    size_t lastSlash = key.find_last_of('/');
    if (lastSlash == std::string::npos) {
        MyLogger::error("Invalid directory key: " + key);
        return false;
    }
    std::string parent_dir_key = key.substr(0, lastSlash);
    Directory_Metadata dir_metadata(parent_dir_key);
    if (!dir_metadata.loadFromDatabase()) {
        MyLogger::error("Failed to load directory metadata from database for: " + parent_dir_key);
        return false;
    }
    // Remove the directory from the parent's list of directories
    auto it = std::remove(dir_metadata._directories.begin(), dir_metadata._directories.end(), key);
    if (it != dir_metadata._directories.end()) {
        dir_metadata._directories.erase(it, dir_metadata._directories.end());
        if (!dir_metadata.storeToDatabase()) {
            MyLogger::error("Failed to update directory metadata in database for: " +
                            parent_dir_key);
            return false;
        }
    } else {
        MyLogger::info("Directory not found in parent directory: " + key);
    }
    return true;
}
}  // namespace metadata
