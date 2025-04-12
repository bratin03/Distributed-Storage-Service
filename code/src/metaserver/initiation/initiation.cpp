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
    inline std::unique_ptr<async_broadcast::AsyncBroadcaster>broadcaster;

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
            std::vector<std::string> group;
            for (const auto& endpoint : value) {
                group.push_back(endpoint.get<std::string>());
            }
            blockserver_lists.push_back(std::move(group));
        }

        MyLogger::info("Successfully initialized all config parameters.");

        broadcaster = std::make_unique<async_broadcast::AsyncBroadcaster>(parseServerUrls(notification_servers));


    }

    std::string loadKey(const std::string &filename) {
        std::ifstream file(filename);
        if (!file.is_open()) {
            MyLogger::error("Failed to open key file: " + filename);
            throw std::runtime_error("Failed to open key file");
        }
        return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
    }

    std::vector<async_broadcast::Server> parseServerUrls(const std::vector<std::string>& urls) {
        std::vector<async_broadcast::Server> servers;
        for (const auto& url : urls) {
            std::string temp = url;
            // Remove protocol (e.g., "http://") if present.
            size_t pos = temp.find("://");
            if (pos != std::string::npos) {
                temp = temp.substr(pos + 3);
            }
            // Find the colon that separates the IP and port.
            pos = temp.find(':');
            if (pos != std::string::npos) {
                std::string ip = temp.substr(0, pos);
                // Parse port number from the remainder of the string.
                unsigned short port = static_cast<unsigned short>(std::stoi(temp.substr(pos + 1)));
                servers.push_back({ ip, port });
            }
            else {
                MyLogger::error("Invalid server URL format: " + url);
            }
        }
        return servers;
    }

}
