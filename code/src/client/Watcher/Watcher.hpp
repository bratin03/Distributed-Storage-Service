// File: Watcher.hpp
#pragma once
#include <string>
#include <functional>
#include <vector>
#include <map>
#include <thread>
#include <atomic>
#include "../Synchronization/APISynchronizer.hpp"

// Watcher class using inotify
class Watcher
{
private:
    int inotify_fd;
    std::atomic<bool> running;
    std::thread watch_thread;
    std::string root_dir;
    std::map<int, std::string> watch_descriptors;
    FileSystemEventHandler *event_handler;

    void watchThread();
    void processEvent(struct inotify_event *event);

public:
    Watcher(const std::string &dir, FileSystemEventHandler *handler);
    ~Watcher();

    void startWatching();
    void stopWatching();
    void addWatch(const std::string &path);
};