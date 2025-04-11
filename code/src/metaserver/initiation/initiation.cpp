#include "initiation.hpp"
#include "../logger/Mylogger.h"
#include <fstream>
#include <mutex>

using json = nlohmann::json;
namespace Initiation {

    inline std::string public_key;
    inline std::string server_ip;
    inline int server_port;

    inline std::vector<std::string> notification_servers;
    inline std::vector<std::vector<std::string>> metastorage_groups;
    inline std::vector<json> blockserver_lists;

    void initialize(const std::string& config_file) {
        std::ifstream file(config_file);
        if (!file.is_open()) {
            MyLogger::error("Failed to open config file: " + config_file);
            throw std::runtime_error("Failed to open config file");
        }

        nlohmann::json config;
        file >> config;

        // Load basic config
        server_ip = config["server_ip"];
        server_port = config["server_port"];
        notification_servers = config["notification_servers"].get<std::vector<std::string>>();

        // Load and parse public key
        public_key = loadKey(config["public_key_file"]);

        // Load metastorage groups
        metastorage_groups.clear();
        for (const auto& [key, value] : config["metastorage_lists"].items()) {
            std::vector<std::string> group;
            for (const auto& endpoint : value) {
                group.push_back(endpoint.get<std::string>());
            }
            metastorage_groups.push_back(std::move(group));
        }

        // Load block storage groups (keep as JSONs for now)
        blockserver_lists.clear();
        for (const auto& [key, value] : config["blockstorage_lists"].items()) {
            blockserver_lists.push_back(value);
        }

        MyLogger::info("Successfully initialized all config parameters.");
    }

    std::string loadKey(const std::string &filename) {
        std::ifstream file(filename);
        if (!file.is_open()) {
            MyLogger::error("Failed to open key file: " + filename);
            throw std::runtime_error("Failed to open key file");
        }
        return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
    }

}
