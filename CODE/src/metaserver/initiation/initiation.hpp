/*
    CS60002 - Distributed Systems
    Term Project - Spring 2025

    * Author 1: Bratin Mondal (21CS10016)
    * Author 2: Soukhin Nayek (21CS10062)
    * Author 3: Swarnabh Mandal (21CS10068)

    * Department of Computer Science and Engineering
    * Indian Institute of Technology, Kharagpur
*/

#pragma once
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

#include "../../notification_server/AsyncBroadcaster.hpp"

using json = nlohmann::json;

namespace Initiation {
// Public config values (filled during initialization)

extern std::string server_ip;
extern int server_port;
extern std::string public_key;

extern std::vector<std::string> notification_servers;
extern std::vector<std::vector<std::string>> metastorage_groups;
extern std::unique_ptr<async_broadcast::AsyncBroadcaster> broadcaster;
extern std::vector<std::vector<std::string>> blockserver_lists;

void initialize(const std::string &server_config_path);
std::vector<async_broadcast::Server> parseServerUrls(const std::vector<std::string> &urls);
std::string loadKey(const std::string &filename);
}  // namespace Initiation
