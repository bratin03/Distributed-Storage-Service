
#include "Watcher.hpp"
#include <sys/inotify.h>
#include <unistd.h>
#include <iostream>
#include <string>
#include <thread>
#include <filesystem>

namespace fs = std::filesystem;

// Watcher implementation
Watcher::Watcher(const std::string& dir, FileSystemEventHandler* handler) 
    : root_dir(dir), event_handler(handler), running(false) {
    inotify_fd = inotify_init();
    if (inotify_fd < 0) {
        std::cerr << "Failed to initialize inotify: " << strerror(errno) << std::endl;
        exit(EXIT_FAILURE);
    }
}

Watcher::~Watcher() {
    stopWatching();
    close(inotify_fd);
}

void Watcher::startWatching() {
    if (running) return;
    
    running = true;
    
    // Add watch for root directory
    addWatch(root_dir);
    
    // Add watches for all subdirectories
    for (const auto& entry : fs::recursive_directory_iterator(root_dir)) {
        if (fs::is_directory(entry)) {
            addWatch(entry.path().string());
        }
    }
    
    // Start the watch thread
    watch_thread = std::thread(&Watcher::watchThread, this);
}

void Watcher::stopWatching() {
    if (!running) return;
    
    running = false;
    
    if (watch_thread.joinable()) {
        watch_thread.join();
    }
    
    // Remove all watches
    for (const auto& [wd, path] : watch_descriptors) {
        inotify_rm_watch(inotify_fd, wd);
    }
    
    watch_descriptors.clear();
}

void Watcher::addWatch(const std::string& path) {
    uint32_t mask = IN_CREATE | IN_DELETE | IN_MODIFY | IN_MOVED_FROM | IN_MOVED_TO;
    int wd = inotify_add_watch(inotify_fd, path.c_str(), mask);
    
    if (wd < 0) {
        std::cerr << "Failed to add watch for " << path << ": " << strerror(errno) << std::endl;
        return;
    }
    
    watch_descriptors[wd] = path;
    std::cout << "Added watch for: " << path << std::endl;
}

void Watcher::watchThread() {
    const size_t EVENT_SIZE = sizeof(struct inotify_event);
    const size_t BUF_LEN = 1024 * (EVENT_SIZE + 16);
    char buffer[BUF_LEN];
    
    while (running) {
        ssize_t length = read(inotify_fd, buffer, BUF_LEN);
        
        if (length < 0) {
            if (errno == EINTR) continue; // Interrupted system call
            std::cerr << "Error reading inotify events: " << strerror(errno) << std::endl;
            break;
        }
        
        // Process all events in the buffer
        ssize_t i = 0;
        while (i < length) {
            struct inotify_event* event = (struct inotify_event*)&buffer[i];
            processEvent(event);
            i += EVENT_SIZE + event->len;
        }
    }
}

void Watcher::processEvent(struct inotify_event* event) {
    if (event->len == 0 || !event_handler) return;
    
    std::string path = watch_descriptors[event->wd];
    std::string name = event->name;
    std::string full_path = path + "/" + name;
    
    // Skip hidden files and directories starting with .dss
    if (name.starts_with(".dss") || name.starts_with(".")) {
        return;
    }
    
    if (event->mask & IN_CREATE) {
        if (event->mask & IN_ISDIR) {
            addWatch(full_path);
            event_handler->onDirectoryCreated(full_path);
        } else {
            event_handler->onFileCreated(full_path);
        }
    } else if (event->mask & IN_DELETE) {
        if (event->mask & IN_ISDIR) {
            event_handler->onDirectoryDeleted(full_path);
        } else {
            event_handler->onFileDeleted(full_path);
        }
    } else if (event->mask & IN_MODIFY) {
        if (!(event->mask & IN_ISDIR)) {
            event_handler->onFileModified(full_path);
        }
    } else if (event->mask & IN_MOVED_FROM) {
        if (event->mask & IN_ISDIR) {
            event_handler->onDirectoryDeleted(full_path);
        } else {
            event_handler->onFileDeleted(full_path);
        }
    } else if (event->mask & IN_MOVED_TO) {
        if (event->mask & IN_ISDIR) {
            addWatch(full_path);
            event_handler->onDirectoryCreated(full_path);
        } else {
            event_handler->onFileCreated(full_path);
        }
    }
}
