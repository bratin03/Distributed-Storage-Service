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

#include <memory>
#include <nlohmann/json.hpp>

#include "../app_notify/app_notify.hpp"
#include "../cache/cache.hpp"
#include "../fsUtils/fsUtils.hpp"
#include "../kv/kv.hpp"
#include "Mylogger.hpp"
#include "../login/login.hpp"
#include "../merge/merge.hpp"
#include "../metadata/metadata.hpp"

using json = nlohmann::json;

namespace serverUtils {
extern std::shared_ptr<cache::Cache> cache_instance;
void initializeCache(std::chrono::milliseconds defaultTTL = std::chrono::minutes(15),
                     std::size_t maxSize = 4096);
json createFile(const std::string &file_key);
bool uploadFile(const std::string &file_key);
json createDir(const std::string &dir_key);
std::vector<std::string> getFileEndpoints(const std::string &file_key);
bool fetchNewFile(const std::string &file_key);
bool deleteFile(const std::string &file_key);
void Conflict(const std::string &file_key);
extern std::string notificationLoadBalancerip;
extern unsigned short notificationLoadBalancerPort;
extern std::string device_id;
}  // namespace serverUtils