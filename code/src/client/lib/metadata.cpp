#include "metadata.h"
#include "leveldb/db.h"
#include "nlohmann/json.hpp"
#include "absl/time/time.h"
#include "absl/time/clock.h"
#include <string>
#include <vector>

// Alias for convenience
using json = nlohmann::json;

// Helper functions to convert between absl::Time and ISO8601 strings.
static std::string TimeToString(absl::Time time)
{
    return absl::FormatTime("%FT%TZ", time, absl::UTCTimeZone());
}

static absl::Time StringToTime(const std::string &timeStr)
{
    absl::Time result;
    std::string err;
    if (absl::ParseTime("%FT%TZ", timeStr, absl::UTCTimeZone(), &result, &err))
    {
        return result;
    }
    return absl::UnixEpoch();
}

// ---------- Chunk_Metadata Implementation ----------

Chunk_Metadata::Chunk_Metadata()
    : chunkName(""),
      localModificationTime(absl::Now()),
      serverModificationTime(absl::Now()),
      chunkSize(0),
      chunkNumber(0),
      weak_Checksum(0),
      strong_Checksum("")
{
}

void Chunk_Metadata::setChunkName(const folly::fbstring &name)
{
    chunkName = name;
}

folly::fbstring Chunk_Metadata::getChunkName() const
{
    return chunkName;
}

bool Chunk_Metadata::storeToDatabase(leveldb::DB *db)
{
    json j;
    j["chunkName"] = chunkName.toStdString();
    j["localModificationTime"] = TimeToString(localModificationTime);
    j["serverModificationTime"] = TimeToString(serverModificationTime);
    j["chunkSize"] = chunkSize;
    j["chunkNumber"] = chunkNumber;
    j["weak_Checksum"] = weak_Checksum;
    j["strong_Checksum"] = strong_Checksum.toStdString();

    std::string key = chunkName.toStdString();
    std::string value = j.dump();

    leveldb::Status status = db->Put(leveldb::WriteOptions(), key, value);
    return status.ok();
}

bool Chunk_Metadata::loadFromDatabase(leveldb::DB *db)
{
    std::string key = chunkName.toStdString();
    std::string value;
    leveldb::Status status = db->Get(leveldb::ReadOptions(), key, &value);
    if (!status.ok())
    {
        return false;
    }
    json j = json::parse(value);
    if (j.contains("chunkName"))
        chunkName = folly::fbstring(j["chunkName"].get<std::string>());
    if (j.contains("localModificationTime"))
        localModificationTime = StringToTime(j["localModificationTime"].get<std::string>());
    if (j.contains("serverModificationTime"))
        serverModificationTime = StringToTime(j["serverModificationTime"].get<std::string>());
    if (j.contains("chunkSize"))
        chunkSize = j["chunkSize"].get<uint64_t>();
    if (j.contains("chunkNumber"))
        chunkNumber = j["chunkNumber"].get<uint64_t>();
    if (j.contains("weak_Checksum"))
        weak_Checksum = j["weak_Checksum"].get<uint32_t>();
    if (j.contains("strong_Checksum"))
        strong_Checksum = folly::fbstring(j["strong_Checksum"].get<std::string>());

    return true;
}

// ---------- File_Metadata Implementation ----------

File_Metadata::File_Metadata()
    : fileName(""),
      localModificationTime(absl::Now()),
      creationTime(absl::Now()),
      fileSize(0),
      numberOfChunks(0)
{
}

void File_Metadata::setFileName(const folly::fbstring &name)
{
    fileName = name;
}

folly::fbstring File_Metadata::getFileName() const
{
    return fileName;
}

bool File_Metadata::storeToDatabase(leveldb::DB *db)
{
    json j;
    j["fileName"] = fileName.toStdString();
    j["localModificationTime"] = TimeToString(localModificationTime);
    j["creationTime"] = TimeToString(creationTime);
    j["fileSize"] = fileSize;
    j["numberOfChunks"] = numberOfChunks;

    std::string key = fileName.toStdString();
    std::string value = j.dump();

    leveldb::Status status = db->Put(leveldb::WriteOptions(), key, value);
    return status.ok();
}

bool File_Metadata::loadFromDatabase(leveldb::DB *db)
{
    std::string key = fileName.toStdString();
    std::string value;
    leveldb::Status status = db->Get(leveldb::ReadOptions(), key, &value);
    if (!status.ok())
    {
        return false;
    }
    json j = json::parse(value);
    if (j.contains("fileName"))
        fileName = folly::fbstring(j["fileName"].get<std::string>());
    if (j.contains("localModificationTime"))
        localModificationTime = StringToTime(j["localModificationTime"].get<std::string>());
    if (j.contains("creationTime"))
        creationTime = StringToTime(j["creationTime"].get<std::string>());
    if (j.contains("fileSize"))
        fileSize = j["fileSize"].get<uint64_t>();
    if (j.contains("numberOfChunks"))
        numberOfChunks = j["numberOfChunks"].get<uint64_t>();

    return true;
}

// ---------- Directory_Metadata Implementation ----------

Directory_Metadata::Directory_Metadata()
    : directoryName(""),
      localModificationTime(absl::Now()),
      serverModificationTime(absl::Now()),
      numberOfFiles(0),
      numberOfDirectories(0)
{
}

void Directory_Metadata::setDirectoryName(const folly::fbstring &name)
{
    directoryName = name;
}

folly::fbstring Directory_Metadata::getDirectoryName() const
{
    return directoryName;
}

bool Directory_Metadata::storeToDatabase(leveldb::DB *db)
{
    json j;
    j["directoryName"] = directoryName.toStdString();
    j["localModificationTime"] = TimeToString(localModificationTime);
    j["serverModificationTime"] = TimeToString(serverModificationTime);
    j["numberOfFiles"] = numberOfFiles;
    j["numberOfDirectories"] = numberOfDirectories;

    std::vector<std::string> filesVec;
    for (const auto &file : files)
    {
        filesVec.push_back(file.toStdString());
    }
    j["files"] = filesVec;

    std::vector<std::string> directoriesVec;
    for (const auto &dir : directories)
    {
        directoriesVec.push_back(dir.toStdString());
    }
    j["directories"] = directoriesVec;

    std::string key = directoryName.toStdString();
    std::string value = j.dump();

    leveldb::Status status = db->Put(leveldb::WriteOptions(), key, value);
    return status.ok();
}

bool Directory_Metadata::loadFromDatabase(leveldb::DB *db)
{
    std::string key = directoryName.toStdString();
    std::string value;
    leveldb::Status status = db->Get(leveldb::ReadOptions(), key, &value);
    if (!status.ok())
    {
        return false;
    }
    json j = json::parse(value);
    if (j.contains("directoryName"))
        directoryName = folly::fbstring(j["directoryName"].get<std::string>());
    if (j.contains("localModificationTime"))
        localModificationTime = StringToTime(j["localModificationTime"].get<std::string>());
    if (j.contains("serverModificationTime"))
        serverModificationTime = StringToTime(j["serverModificationTime"].get<std::string>());
    if (j.contains("numberOfFiles"))
        numberOfFiles = j["numberOfFiles"].get<uint64_t>();
    if (j.contains("numberOfDirectories"))
        numberOfDirectories = j["numberOfDirectories"].get<uint64_t>();

    if (j.contains("files"))
    {
        files.clear();
        for (const auto &file : j["files"])
        {
            files.push_back(folly::fbstring(file.get<std::string>()));
        }
    }
    if (j.contains("directories"))
    {
        directories.clear();
        for (const auto &dir : j["directories"])
        {
            directories.push_back(folly::fbstring(dir.get<std::string>()));
        }
    }

    return true;
}
