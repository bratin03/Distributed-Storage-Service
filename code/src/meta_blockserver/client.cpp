#include <iostream>
#include <vector>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <nlohmann/json.hpp>
#include "../../../utils/Distributed_KV/client_lib/kv.hpp"

using json = nlohmann::json;
using namespace distributed_KV;

// Helper function to read the entire config file and extract servers, filePath, and version.
void loadConfig(const std::string &configPath,
                std::vector<std::string> &servers,
                std::string &filePath,
                std::string &version) {
    std::ifstream configFile(configPath);
    if (!configFile) {
        throw std::runtime_error("Failed to open config file: " + configPath);
    }
    
    json config;
    configFile >> config;
    
    // Read servers array from config
    if (config.contains("servers") && config["servers"].is_array()) {
        for (const auto &server : config["servers"]) {
            servers.push_back(server.get<std::string>());
        }
    } else {
        throw std::runtime_error("Invalid config file: 'servers' key missing or not an array");
    }
    
    // Read filePath and version from config
    if (config.contains("filePath") && config.contains("version")) {
        filePath = config["filePath"].get<std::string>();
        version = config["version"].get<std::string>();
    } else {
        throw std::runtime_error("Config file must contain 'filePath' and 'version' keys.");
    }
}

int main() {
    std::vector<std::string> servers;
    std::string filePath, version;
    
    try {
        // Load configuration from config.json
        loadConfig("config.json", servers, filePath, version);
    } catch (const std::exception &e) {
        std::cerr << "Error loading config: " << e.what() << std::endl;
        return 1;
    }
    
    // Set the file content into the distributed KV store using filePath as the key.
    std::cout << "Setting file '" << filePath << "' with version " << version << "...\n";
    auto setResp = setFile(servers, filePath, version);
    if (setResp.success) {
        std::cout << "Set succeeded: " << setResp.value << std::endl;
    } else {
        std::cerr << "Set failed: " << setResp.err << std::endl;
    }
    
    // Get the stored file content.
    std::cout << "Getting file '" << filePath << "'...\n";
    auto getResp = getFile(servers, filePath);
    if (getResp.success) {
        std::cout << "Get succeeded: " << getResp.value << std::endl;
    } else {
        std::cerr << "Get failed: " << getResp.err << std::endl;
    }
    
    // // Delete the file key from the store.
    // std::cout << "Deleting file key '" << filePath << "'...\n";
    // auto delResp = deleteFile(servers, filePath);
    // if (delResp.success) {
    //     std::cout << "Delete succeeded: " << delResp.value << std::endl;
    // } else {
    //     std::cerr << "Delete failed: " << delResp.err << std::endl;
    // }
    
    return 0;
}
