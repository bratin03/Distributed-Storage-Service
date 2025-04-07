#include <iostream>
#include <queue>
#include <set>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <memory>
#include <string>
#include "recursive_inotify.hpp"

// Helper function to convert InotifyEventType to a string.
std::string eventTypeToString(InotifyEventType type)
{
    switch (type)
    {
    case InotifyEventType::Created:
        return "Created";
    case InotifyEventType::Deleted:
        return "Deleted";
    case InotifyEventType::Modified:
        return "Modified";
    case InotifyEventType::MovedFrom:
        return "MovedFrom";
    case InotifyEventType::MovedTo:
        return "MovedTo";
    default:
        return "Other";
    }
}

// Helper function to convert FileType to a string.
std::string fileTypeToString(FileType ft)
{
    return (ft == FileType::Directory) ? "Directory" : "File";
}

int main(int argc, char *argv[])
{
    if (argc < 2)
    {
        std::cerr << "Usage: " << argv[0] << " <path_to_watch>" << std::endl;
        return 1;
    }

    std::string root_dir = argv[1];

    // Shared data structures used for communication between threads.
    auto eventQueue = std::make_shared<std::queue<FileEvent>>();
    auto eventMap = std::make_shared<std::set<FileEvent>>();
    auto mtx = std::make_shared<std::mutex>();
    auto cv = std::make_shared<std::condition_variable>();

    // Start the directory watcher in a separate thread.
    std::thread watcher_thread(watch_directory, root_dir, eventQueue, eventMap, mtx, cv);

    // Main thread: Process and print events as they arrive.
    while (true)
    {
        std::unique_lock<std::mutex> lock(*mtx);
        // Wait until there is at least one event in the queue.
        cv->wait(lock, [&]
                 { return !eventQueue->empty(); });

        while (!eventQueue->empty())
        {
            FileEvent fevent = eventQueue->front();
            eventQueue->pop();
            // Delete the event from the map to avoid duplicates.
            eventMap->erase(fevent);
            // Unlock while processing the event to allow the watcher thread to push new events.
            lock.unlock();

            // Print event details.
            std::cout << "Event: " << eventTypeToString(fevent.eventType)
                      << " | Path: " << fevent.path
                      << " | Type: " << fileTypeToString(fevent.fileType)
                      << std::endl;

            // Re-lock for the next iteration.
            lock.lock();
        }
    }

    // Although in this infinite loop we never reach here,
    // join the watcher thread before exiting in a real application.
    watcher_thread.join();
    return 0;
}
