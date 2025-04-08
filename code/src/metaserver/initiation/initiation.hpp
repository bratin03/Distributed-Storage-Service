#pragma once
#include <string>
#include <vector>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace Initiation
{
    // Public config values (filled during initialization)
   
   
    extern std::string server_ip;
    extern int server_port;
    extern std::string public_key;

    extern std::vector<std::string> metadata_servers;
    extern std::vector<std::string> notification_servers;
    extern std::vector<std::vector<std::string>> metastorage_groups;
    inline std::vector<json> blockserver_lists = {};

    void initialize(const std::string& server_config_path);

    std::string loadKey(const std::string& filename);
}
