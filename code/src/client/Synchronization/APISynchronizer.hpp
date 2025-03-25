#pragma once
#include <string>
#include <vector>
#include <map>
#include <nlohmann/json.hpp>
#include <curl/curl.h>
#include "../../../utils/libraries/cpp-httplib/httplib.h"
#include "../Indexer/Indexer.hpp"
#include "../Chunker/Chunker.hpp"
using json = nlohmann::json;

// Event handler interface
class FileSystemEventHandler
{
public:
    virtual void onFileCreated(const std::string &path) = 0;
    virtual void onFileModified(const std::string &path) = 0;
    virtual void onFileDeleted(const std::string &path) = 0;
    virtual void onDirectoryCreated(const std::string &path) = 0;
    virtual void onDirectoryDeleted(const std::string &path) = 0;
    virtual ~FileSystemEventHandler() {}
};

// HTTP API client
class ApiClient
{
private:
    std::string server_url;
    std::unique_ptr<httplib::Client> cli;

    static size_t WriteCallback(void *contents, size_t size, size_t nmemb, void *userp);

public:
    ApiClient(const std::string &url);
    ~ApiClient();

    json createDirectory(const std::string &dir_id);
    json deleteDirectory(const std::string &dir_id);
    json listDirectory(const std::string &dir_id);

    json deleteFile(const std::string &file_id);
    json downloadFile(const std::string &file_id);
    json downloadChunk(const std::string &chunk_id);

    json updateRequest(int file_version, const FileMetadata &metadata);
    json storeChunk(const std::string &chunk_id, const json &metadata, const std::string &data);
    json commitUpdate(const FileMetadata &metadata);

    json handleResponse(const httplib::Result& res);
};

// Synchronizer class to coordinate file synchronization
class Synchronizer : public FileSystemEventHandler
{
private:
    std::string root_dir;
    std::string chunks_dir;
    Indexer *indexer;
    Chunker *chunker;
    ApiClient *api_client;

public:
    Synchronizer(const std::string &root, const std::string &chunks, Indexer *idx, Chunker *chk, ApiClient *api);

    void initialSync();
    void syncDirectory(const std::string &dir_id);
    void uploadFile(const std::string &file_path);
    void downloadFile(const std::string &file_id);

    // FileSystemEventHandler interface implementation
    void onFileCreated(const std::string &path) override;
    void onFileModified(const std::string &path) override;
    void onFileDeleted(const std::string &path) override;
    void onDirectoryCreated(const std::string &path) override;
    void onDirectoryDeleted(const std::string &path) override;
};