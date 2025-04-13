#pragma once
#include "../watcher/watcher.hpp"
#include "../logger/Mylogger.hpp"
#include "../metadata/metadata.hpp"
#include "../fsUtils/fsUtils.hpp"
#include "../serverUtils/serverUtils.hpp"

namespace process_local
{
    extern unsigned int wait_threshold;
    extern unsigned int wait_time;

    // Function to process local events
    void process_local_events(
        std::queue<watcher::FileEvent> &eventQueue,
        std::set<watcher::FileEvent> &eventMap,
        std::mutex &mtx,
        std::condition_variable &cv,
        std::mutex &db_mutex);

    void process_event(watcher::FileEvent &event);

    void delete_event(const std::string &path, watcher::FileType filetype);
    void create_file(const std::string &path);
    void create_directory(const std::string &path);
} // namespace process_local