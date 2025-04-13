#include "process_local.hpp"
#include <chrono>
#include <condition_variable>
#include <iostream>
#include <mutex>
#include <queue>
#include <set>

namespace process_local
{

    unsigned int wait_threshold = 5; // Minimum events to trigger immediate processing
    unsigned int wait_time = 5;      // Maximum wait time in milliseconds

    void process_local_events(
        std::queue<watcher::FileEvent> &eventQueue,
        std::set<watcher::FileEvent> &eventMap,
        std::mutex &mtx,
        std::condition_variable &cv)
    {
        while (true)
        {
            std::unique_lock<std::mutex> lock(mtx);

            // Wait until either the number of events in the queue is at least wait_threshold,
            // or until wait_time (in milliseconds) has passed.
            cv.wait_for(lock,
                        std::chrono::milliseconds(wait_time),
                        [&eventQueue]()
                        { return eventQueue.size() >= wait_threshold; });

            // At this point, either the queue size threshold is reached (predicate_met is true)
            // or the timeout has expired (predicate_met is false). In either case, process the queued events.
            while (!eventQueue.empty())
            {
                watcher::FileEvent event = eventQueue.front();
                eventQueue.pop();
                eventMap.erase(event); // Remove the event from the set
                process_event(event);
            }
        }
    }

    void process_event(watcher::FileEvent &event)
    {
        MyLogger::info("Processing event: " + std::to_string(static_cast<int>(event.eventType)) +
                       " for path: " + event.path);
    }
}
