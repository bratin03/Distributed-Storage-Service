#pragma once
#include "../notification/notification.hpp"
#include "../logger/Mylogger.hpp"
#include <nlohmann/json.hpp>
#include "../serverUtils/serverUtils.hpp"
#include "../metadata/metadata.hpp"
#include "../fsUtils/fsUtils.hpp"
#include "../kv/kv.hpp"
#include "../login/login.hpp"

using json = nlohmann::json;

namespace process_remote
{
    void process_remote_events(
        std::queue<json> &eventQueue,
        std::mutex &mtx,
        std::condition_variable &cv,
        std::mutex &db_mutex);
    void process_event(const json &event);

}
