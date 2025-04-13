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
        std::condition_variable &cv,
        std::mutex &db_mutex)
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
                db_mutex.lock();
                process_event(event);
                db_mutex.unlock();
            }
        }
    }

    void process_event(watcher::FileEvent &event)
    {
        MyLogger::info("Processing event: " + std::to_string(static_cast<int>(event.eventType)) +
                       " for path: " + event.path);
        auto &eventType = event.eventType;
        if (eventType == watcher::InotifyEventType::Created ||
            eventType == watcher::InotifyEventType::MovedTo)
        {
            auto key = fsUtils::buildKeyfromFullPath(event.path);
            if(event.fileType == watcher::FileType::File)
            {
                create_file(key);
            }
            else if(event.fileType == watcher::FileType::Directory)
            {
                create_directory(key);
            }
            else
            {
                MyLogger::error("Unknown file type for path: " + event.path);
            }
        }
        else if (eventType == watcher::InotifyEventType::Modified)
        {
        }
        else if (eventType == watcher::InotifyEventType::Deleted ||
                 eventType == watcher::InotifyEventType::MovedFrom)
        {
            auto key = fsUtils::buildKeyfromFullPath(event.path);
            delete_event(key, event.fileType);
        }
    }

    void delete_event(const std::string &path, watcher::FileType filetype)
    {
        if (filetype == watcher::FileType::File)
        {
            MyLogger::info("Deleting file: " + path);
            metadata::removeFileFromDatabase(path);
            metadata::removeFileFromDirectory(path);
            serverUtils::deleteFile(path);
        }
        else if (filetype == watcher::FileType::Directory)
        {
            MyLogger::info("Deleting directory: " + path);
            metadata::removeDirectoryFromDatabase(path);
            metadata::removeDirectoryFromDirectory(path);
            serverUtils::deleteFile(path);
        }
        else
        {
            MyLogger::error("Unknown file type for path: " + path);
        }
    }

    void create_file(const std::string &path)
    {
        MyLogger::info("Creating file: " + path);

        metadata::File_Metadata fileMetadata(path);
        if(fileMetadata.loadFromDatabase())
        {
            MyLogger::info("File already exists in database: " + path);
            return;
        }
        fileMetadata.storeToDatabase();

        metadata::addFileToDirectory(path);
        serverUtils::createFile(path);
        serverUtils::uploadFile(path);
        MyLogger::info("File created and uploaded: " + path);
    }

    void create_directory(const std::string &path)
    {
        MyLogger::info("Creating directory: " + path);

        metadata::Directory_Metadata dirMetadata(path);
        if(dirMetadata.loadFromDatabase())
        {
            MyLogger::info("Directory already exists in database: " + path);
            return;
        }

        metadata::addDirectoryToDirectory(path);
        serverUtils::createDir(path);
        MyLogger::info("Directory created: " + path);
    }
}
