
#include "Watcher.hpp"
#include <sys/inotify.h>
#include <unistd.h>
#include <iostream>
#include <string>
#include <thread>
#include <filesystem>
#include <inotify-cpp/NotifierBuilder.h>
#include <mutex>

namespace fs = std::filesystem;

// Watcher implementation
Watcher::Watcher(const std::string &dir, FileSystemEventHandler *handler)
    : root_dir(dir), event_handler(handler), running(false)
{
    // Setup initial watch
    notifier = inotify::BuildNotifier()
                   .watchPathRecursively(root_dir)
                   .ignoreFileOnce(".*") // Ignore hidden files
                   .onEvents({inotify::Event::create,
                              inotify::Event::modify,
                              inotify::Event::remove,
                              inotify::Event::moved_from,
                              inotify::Event::moved_to},
                             [this](const inotify::Notification &notification)
                             {
                                 handleEvent(notification);
                             })
                   .build();
}

Watcher::~Watcher()
{
    stopWatching();
}

void Watcher::startWatching()
{
    running = true;
    watch_thread = std::thread([this]()
                               {
        while (running) {
            {
                std::lock_guard<std::mutex> lock(notifier_mutex);
                notifier->runOnce();
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        } });
}

void Watcher::stopWatching()
{
    running = false;
    if (watch_thread.joinable())
    {
        watch_thread.join();
    }
}

void Watcher::addWatch(const std::string &path)
{
    std::lock_guard<std::mutex> lock(notifier_mutex);
    try
    {
        if (fs::is_directory(path))
        {
            notifier->watchPathRecursively(path);
            std::cout << "Added watch for: " << path << std::endl;
        }
    }
    catch (const std::exception &e)
    {
        std::cerr << "Failed to add watch for " << path
                  << ": " << e.what() << std::endl;
    }
}

void Watcher::handleEvent(const inotify::Notification &notification)
{
    const auto &path = notification.path;

    // Skip hidden files and system files
    if (path.filename().string().starts_with(".") ||
        path.string().find("/.") != std::string::npos)
    {
        return;
    }

    if (notification.event & inotify::Event::create)
    {
        if (fs::is_directory(path))
        {
            addWatch(path.string()); // Add watch for new directory
            event_handler->onDirectoryCreated(path.string());
        }
        else
        {
            event_handler->onFileCreated(path.string());
        }
    }
    else if (notification.event & inotify::Event::modify)
    {
        if (fs::is_regular_file(path))
        {
            event_handler->onFileModified(path.string());
        }
    }
    else if (notification.event & inotify::Event::remove)
    {
        if (notification.event & inotify::Event::is_dir)
        {
            event_handler->onDirectoryDeleted(path.string());
        }
        else
        {
            event_handler->onFileDeleted(path.string());
        }
    }
    else if (notification.event & inotify::Event::moved_from)
    {
        if (fs::is_directory(path))
        {
            event_handler->onDirectoryDeleted(path.string());
        }
        else
        {
            event_handler->onFileDeleted(path.string());
        }
    }
    else if (notification.event & inotify::Event::moved_to)
    {
        if (fs::is_directory(path))
        {
            addWatch(path.string());
            event_handler->onDirectoryCreated(path.string());
        }
        else
        {
            event_handler->onFileCreated(path.string());
        }
    }
}
