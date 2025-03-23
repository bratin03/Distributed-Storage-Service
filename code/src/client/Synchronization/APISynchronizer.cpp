#define CPPHTTPLIB_OPENSSL_SUPPORT
#include "APISynchronizer.hpp"
#include "../utils/utils.hpp"
#include <iostream>
#include <sstream>
#include <thread>
#include <chrono>
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

// ApiClient implementation using cpp-httplib
ApiClient::ApiClient(const std::string &url) : server_url(url)
{
    if (server_url.find("https://") != std::string::npos)
    {
        // cli = std::make_unique<httplib::SSLClient>(server_url);
        cli = std::make_unique<httplib::Client>(server_url); // httplib:SSLclient not able to assigned to cli
    }
    else
    {
        cli = std::make_unique<httplib::Client>(server_url);
    }
    cli->set_connection_timeout(30);
    cli->set_read_timeout(45);
    cli->set_write_timeout(45);
}

// ApiClient destructor
ApiClient::~ApiClient()
{
    // Nothing to do
}

// ApiClient file operations

json ApiClient::createDirectory(const std::string &dir_id)
{
    std::string path = "/create_directory/" + httplib::detail::encode_url(dir_id);
    auto res = cli->Post(path.c_str());
    return handleResponse(res);
}

json ApiClient::deleteDirectory(const std::string &dir_id)
{
    std::string path = "/delete_directory/" + httplib::detail::encode_url(dir_id);
    auto res = cli->Delete(path.c_str());
    return handleResponse(res);
}

json ApiClient::listDirectory(const std::string &dir_id)
{
    std::string path = "/list_directory/" + httplib::detail::encode_url(dir_id);
    auto res = cli->Get(path.c_str());
    return handleResponse(res);
}

json ApiClient::deleteFile(const std::string &file_id)
{
    std::string path = "/delete_file/" + httplib::detail::encode_url(file_id);
    auto res = cli->Delete(path.c_str());
    return handleResponse(res);
}

json ApiClient::downloadFile(const std::string &file_id)
{
    std::string path = "/download_file/" + httplib::detail::encode_url(file_id);
    auto res = cli->Get(path.c_str());
    return handleResponse(res);
}

json ApiClient::downloadChunk(const std::string &chunk_id)
{
    std::string path = "/download_chunk/" + httplib::detail::encode_url(chunk_id);
    auto res = cli->Get(path.c_str());
    return handleResponse(res);
}

json ApiClient::updateRequest(int file_version, const FileMetadata &metadata)
{
    json payload;
    payload["file_version"] = file_version;
    payload["metadata"] = metadata;
    auto res = cli->Post("/update_request", payload.dump(), "application/json");
    return handleResponse(res);
}

json ApiClient::storeChunk(const std::string &chunk_id, const json &metadata, const std::string &data)
{
    json payload;
    payload["chunkid"] = chunk_id;
    payload["metadata"] = metadata;
    payload["data"] = httplib::detail::base64_encode(data);

    auto res = cli->Post("/store_chunk", payload.dump(), "application/json");
    return handleResponse(res);
}

json ApiClient::commitUpdate(const FileMetadata &metadata)
{
    json payload;
    payload["metadata"] = metadata;
    auto res = cli->Post("/commit_update", payload.dump(), "application/json");
    return handleResponse(res);
}

// Helper function for API responses
json ApiClient::handleResponse(const httplib::Result &res)
{
    if (!res)
    {
        auto err = res.error();
        throw std::runtime_error("HTTP error: " + httplib::to_string(err));
    }

    if (res->status >= 400)
    {
        throw std::runtime_error("API error: " + res->body);
    }

    return json::parse(res->body);
}

// Synchronizer implementation

// Synchronizer constructor implementation
Synchronizer::Synchronizer(const std::string &root,
                           const std::string &chunks,
                           Indexer *idx,
                           Chunker *chk,
                           ApiClient *api)
    : root_dir(root),
      chunks_dir(chunks),
      indexer(idx),
      chunker(chk),
      api_client(api)
{
    // Validate critical dependencies
    if (!indexer)
    {
        throw std::invalid_argument("Indexer cannot be null");
    }
    if (!chunker)
    {
        throw std::invalid_argument("Chunker cannot be null");
    }
    if (!api_client)
    {
        throw std::invalid_argument("ApiClient cannot be null");
    }

    // Ensure chunks directory exists
    if (!fs::exists(chunks_dir))
    {
        fs::create_directories(chunks_dir);
    }

    // Verify root directory exists
    if (!fs::exists(root_dir))
    {
        throw std::runtime_error("Root directory does not exist: " + root_dir);
    }

    // Convert to absolute paths
    root_dir = fs::absolute(root_dir).string();
    chunks_dir = fs::absolute(chunks_dir).string();

    std::cout << "Initialized Synchronizer:\n"
              << " - Root: " << root_dir << "\n"
              << " - Chunks: " << chunks_dir << "\n";
}

void Synchronizer::initialSync()
{
    // Sync directory structure first
    syncDirectory(".");

    // Then sync all files
    auto root_meta = indexer->getDirMetadata(".");
    for (const auto &file_id : root_meta.files)
    {
        downloadFile(file_id);
    }
}

void Synchronizer::syncDirectory(const std::string &dir_id)
{
    try
    {
        // Get local directory metadata
        auto local_meta = indexer->getDirMetadata(dir_id);

        // Get server directory state
        json server_response = api_client->listDirectory(dir_id);

        // Sync subdirectories
        for (const auto &server_dir : server_response["dir"])
        {
            if (std::find(local_meta.subdirs.begin(), local_meta.subdirs.end(),
                          server_dir["dir_id"]) == local_meta.subdirs.end())
            {
                // Create missing directory
                api_client->createDirectory(server_dir["dir_id"]);
                indexer->updateDirectoryMetadata(server_dir["dir_id"]);
            }
        }

        // Sync files
        for (const auto &server_file : server_response["file"])
        {
            std::string file_id = server_file["file_id"];
            if (std::find(local_meta.files.begin(), local_meta.files.end(),
                          file_id) == local_meta.files.end())
            {
                downloadFile(file_id);
            }
        }
    }
    catch (const std::exception &e)
    {
        std::cerr << "Directory sync failed: " << e.what() << std::endl;
    }
}

void Synchronizer::uploadFile(const std::string &file_path)
{
    // Get relative path and metadata
    fs::path rel_path = fs::relative(file_path, root_dir);
    std::string file_id = rel_path.string();
    try
    {
        FileMetadata local_meta = indexer->getFileMetadata(file_id);

        // Split file into chunks
        auto chunks = chunker->splitFile(file_path, chunks_dir);
        local_meta.chunks = chunks;
        local_meta.overall_hash = chunker->calculateFileHash(chunks);

        // Start update process
        json response = api_client->updateRequest(local_meta.version_number, local_meta);

        if (response.contains("nb"))
        {
            // Upload needed chunks
            for (const auto &chunk_id : response["nb"])
            {
                std::string chunk_path = chunks_dir + "/" + chunk_id.get<std::string>();
                std::ifstream file(chunk_path, std::ios::binary);
                std::string data((std::istreambuf_iterator<char>(file)),
                                 std::istreambuf_iterator<char>());

                json chunk_meta;
                chunk_meta["chunk_id"] = chunk_id;
                chunk_meta["offset"] = local_meta.chunks[0].offset; // Simplified

                api_client->storeChunk(chunk_id, chunk_meta, data);
            }
        }

        // Commit the update
        api_client->commitUpdate(local_meta);

        // Update local metadata
        local_meta.status = "complete";
        indexer->updateFileMetadata(file_path);
    }
    catch (const std::exception &e)
    {
        std::cerr << "File upload failed: " << e.what() << std::endl;
        indexer->getFileMetadata(file_id).status = "error";
    }
}

void Synchronizer::downloadFile(const std::string &file_id)
{
    try
    {
        // Get server metadata
        json server_meta = api_client->downloadFile(file_id);

        // Create temporary directory for chunks
        std::string temp_dir = chunks_dir + "/temp_" + file_id;
        fs::create_directories(temp_dir);

        // Download all chunks
        std::vector<ChunkMetadata> chunks;
        for (const auto &chunk_id : server_meta["chunks"])
        {
            json chunk_data = api_client->downloadChunk(chunk_id);

            // Save chunk to temp directory
            std::string chunk_path = temp_dir + "/" + chunk_id.get<std::string>();
            std::ofstream file(chunk_path, std::ios::binary);
            std::string data = base64_decode(chunk_data["data"]);
            file.write(data.c_str(), data.size());

            // Add chunk metadata
            ChunkMetadata meta;
            meta.chunk_id = chunk_id;
            meta.offset = chunk_data["offset"];
            meta.size = data.size();
            chunks.push_back(meta);
        }

        // Reassemble file
        std::string output_path = root_dir + "/" + file_id;
        chunker->reassembleFile(output_path, chunks, temp_dir);

        // Update local metadata
        FileMetadata local_meta;
        local_meta.file_id = file_id;
        local_meta.version_number = server_meta["version_number"];
        local_meta.timestamp = server_meta["timestamp"];
        local_meta.chunks = chunks;
        local_meta.status = "complete";
        indexer->updateFileMetadata(output_path);

        // Cleanup temp directory
        fs::remove_all(temp_dir);
    }
    catch (const std::exception &e)
    {
        std::cerr << "File download failed: " << e.what() << std::endl;
    }
}

// Event handlers
void Synchronizer::onFileCreated(const std::string &path)
{
    if (fs::is_directory(path))
    {
        syncDirectory(fs::relative(path, root_dir).string());
    }
    else
    {
        uploadFile(path);
    }
}

void Synchronizer::onFileModified(const std::string &path)
{
    uploadFile(path);
}

void Synchronizer::onFileDeleted(const std::string &path)
{
    std::string file_id = fs::relative(path, root_dir).string();
    api_client->deleteFile(file_id);
    indexer->removeFile(path);
}

void Synchronizer::onDirectoryCreated(const std::string &path)
{
    try
    {
        // Get relative path from root
        fs::path rel_path = fs::relative(path, root_dir);
        std::string dir_id = rel_path.string();

        // Create directory on server
        api_client->createDirectory(dir_id);

        // Update local metadata
        indexer->updateDirectoryMetadata(path);
        indexer->saveMetadata();

        // Sync new directory contents
        syncDirectory(dir_id);
    }
    catch (const std::exception &e)
    {
        std::cerr << "Directory creation failed: " << e.what() << std::endl;
    }
}

void Synchronizer::onDirectoryDeleted(const std::string &path)
{
    try
    {
        // Get relative path from root
        fs::path rel_path = fs::relative(path, root_dir);
        std::string dir_id = rel_path.string();

        // Delete directory on server
        api_client->deleteDirectory(dir_id);

        // Update local metadata
        indexer->removeDirectory(path);
        indexer->saveMetadata();
    }
    catch (const std::exception &e)
    {
        std::cerr << "Directory deletion failed: " << e.what() << std::endl;
    }
}