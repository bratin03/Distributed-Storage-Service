
// dss_client.cpp
#include "dss_client.hpp"
#include <unistd.h>
#include <cstring>
#include <chrono>
#include <algorithm>
#include <sstream>
#include <iomanip>

DssClient::DssClient(const std::string &root, const std::string &server)
    : root_dir(root),
      server_url(server),
      chunks_dir(root + "/.dss/chunks"),
      metadata_dir(root + "/.dss/metadata"),
      watcher(nullptr),
      chunker(nullptr),
      indexer(nullptr),
      api_client(nullptr),
      synchronizer(nullptr)
{
    // Ensure root directory exists
    fs::create_directories(root_dir);
}

DssClient::~DssClient()
{
    stop(); // Ensure clean shutdown
    delete synchronizer;
    delete watcher;
    delete api_client;
    delete chunker;
    delete indexer;
}

void DssClient::initialize()
{
    // Create necessary directories
    fs::create_directories(chunks_dir);
    fs::create_directories(metadata_dir);

    // Initialize components
    indexer = new Indexer(root_dir, metadata_dir);
    chunker = new Chunker();
    api_client = new ApiClient(server_url);

    // Load existing metadata
    indexer->loadMetadata();

    // Initial filesystem scan
    indexer->scanDirectory(root_dir);

    // Create synchronizer and watcher
    synchronizer = new Synchronizer(root_dir, chunks_dir, indexer, chunker, api_client);
    watcher = new Watcher(root_dir, synchronizer);

    // Save initial metadata state
    indexer->saveMetadata();

    std::cout << "Client initialized successfully\n";
    std::cout << "Chunks stored in: " << chunks_dir << "\n";
    std::cout << "Metadata stored in: " << metadata_dir << "\n";
}

void DssClient::start()
{
    if (!watcher || !synchronizer)
    {
        throw std::runtime_error("Client not initialized properly");
    }

    std::cout << "Starting file watcher...\n";
    watcher->startWatching();

    std::cout << "Performing initial synchronization...\n";
    synchronizer->initialSync();
    indexer->saveMetadata();

    std::cout << "Client ready. Monitoring changes in: " << root_dir << "\n";
}

void DssClient::stop()
{
    if (watcher)
    {
        std::cout << "Stopping file watcher...\n";
        watcher->stopWatching();
    }

    if (indexer)
    {
        std::cout << "Saving final metadata state...\n";
        indexer->saveMetadata();
    }

    // Clean up temporary chunk files
    fs::remove_all(chunks_dir + "/temp");
    std::cout << "Cleaned up temporary files\n";
}
