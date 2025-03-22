#pragma once

#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <unordered_map>
#include <fstream>
#include <filesystem>
#include <thread>
#include <mutex>
#include <atomic>
#include <condition_variable>
#include <nlohmann/json.hpp>
#include "Watcher/Watcher.hpp"
#include "Chunker/Chunker.hpp"
#include "Indexer/Indexer.hpp"
#include "Synchronization/APISynchronizer.hpp"

namespace fs = std::filesystem;
using json = nlohmann::json;


// Main client class
class DssClient
{
private:
    std::string root_dir;
    std::string chunks_dir;
    std::string metadata_dir;
    std::string server_url;

    Watcher *watcher;
    Chunker *chunker;
    Indexer *indexer;
    ApiClient *api_client;
    Synchronizer *synchronizer;

public:
    DssClient(const std::string &root, const std::string &server);
    ~DssClient();

    void initialize();
    void start();
    void stop();
};