#ifndef METADATA_H
#define METADATA_H

#include <folly/FBString.h>
#include <folly/FBVector.h>
#include "absl/time/time.h"
#include "leveldb/db.h"
#include "nlohmann/json.hpp"

class Chunk_Metadata {
public:
    Chunk_Metadata();

    void setChunkName(const folly::fbstring &name);
    folly::fbstring getChunkName() const;

    // Database methods:
    bool storeToDatabase(leveldb::DB* db);
    bool loadFromDatabase(leveldb::DB* db);

private:
    folly::fbstring chunkName;
    absl::Time localModificationTime;
    absl::Time serverModificationTime;
    uint64_t chunkSize;
    uint64_t chunkNumber;
    uint32_t weak_Checksum;
    folly::fbstring strong_Checksum;
};

class File_Metadata {
public:
    File_Metadata();

    void setFileName(const folly::fbstring &name);
    folly::fbstring getFileName() const;

    // Database methods:
    bool storeToDatabase(leveldb::DB* db);
    bool loadFromDatabase(leveldb::DB* db);

private:
    folly::fbstring fileName;
    absl::Time localModificationTime;
    absl::Time creationTime;
    uint64_t fileSize;
    uint64_t numberOfChunks;
};

class Directory_Metadata {
public:
    Directory_Metadata();

    void setDirectoryName(const folly::fbstring &name);
    folly::fbstring getDirectoryName() const;

    // Database methods:
    bool storeToDatabase(leveldb::DB* db);
    bool loadFromDatabase(leveldb::DB* db);

private:
    folly::fbstring directoryName;
    absl::Time localModificationTime;
    absl::Time serverModificationTime;
    uint64_t numberOfFiles;
    uint64_t numberOfDirectories;
    folly::fbvector<folly::fbstring> files;
    folly::fbvector<folly::fbstring> directories;
};

#endif // METADATA_H
