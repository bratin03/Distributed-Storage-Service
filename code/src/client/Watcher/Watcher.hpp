// File: Watcher.hpp
#pragma once
#include <string>
#include <functional>
#include <vector>
#include <map>
#include <thread>
#include <atomic>
#include "../Synchronization/APISynchronizer.hpp"
#include <inotify-cpp/NotifierBuilder.h>

// Watcher class using inotify
class Watcher
{
private:
    std::unique_ptr<inotify::Notifier> notifier;
    std::thread watch_thread;
    std::atomic<bool> running;
    FileSystemEventHandler *event_handler;
    std::mutex notifier_mutex;
    std::string root_dir;

    void handleEvent(const inotify::Notification& notification);

public:
    Watcher(const std::string &dir, FileSystemEventHandler *handler);
    ~Watcher();

    void startWatching();
    void stopWatching();
    void addWatch(const std::string &path);
};