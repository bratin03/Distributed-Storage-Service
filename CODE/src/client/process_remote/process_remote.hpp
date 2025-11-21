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

#include "../fsUtils/fsUtils.hpp"
#include "../kv/kv.hpp"
#include "Mylogger.hpp"
#include "../login/login.hpp"
#include "../metadata/metadata.hpp"
#include "../notification/notification.hpp"
#include "../serverUtils/serverUtils.hpp"

using json = nlohmann::json;

namespace process_remote {
void process_remote_events(std::queue<json> &eventQueue, std::mutex &mtx,
                           std::condition_variable &cv, std::mutex &db_mutex);
void process_event(const json &event);

}  // namespace process_remote
