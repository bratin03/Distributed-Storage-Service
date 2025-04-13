#pragma once
#include "../watcher/watcher.hpp"
#include "../logger/Mylogger.hpp"

namespace process_local
{
    extern unsigned int wait_threshold;
    extern unsigned int wait_time;

    // Function to process local events
    void process_local_events(
        std::queue<watcher::FileEvent> &eventQueue,
        std::set<watcher::FileEvent> &eventMap,
        std::mutex &mtx,
        std::condition_variable &cv);

    void process_event(watcher::FileEvent &event);
} // namespace process_local