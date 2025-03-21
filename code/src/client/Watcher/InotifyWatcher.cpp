
// File: InotifyWatcher.cpp (Linux example)
#ifdef __linux__
#include "Watcher.hpp"
#include <sys/inotify.h>
#include <unistd.h>
#include <iostream>
#include <cstring>
#include <thread>

class InotifyWatcher : public Watcher {
public:
    void startWatching(const std::string& directory, Callback callback) override {
        int fd = inotify_init();
        if (fd < 0) {
            std::cerr << "inotify_init error" << std::endl;
            return;
        }
        int wd = inotify_add_watch(fd, directory.c_str(), IN_MODIFY | IN_CREATE | IN_DELETE);
        if (wd < 0) {
            std::cerr << "inotify_add_watch error: " << strerror(errno) << std::endl;
            close(fd);
            return;
        }
        // Run in a separate thread
        std::thread([fd, callback](){
            const size_t bufSize = 1024 * (sizeof(inotify_event) + 16);
            char buffer[bufSize];
            while (true) {
                int length = read(fd, buffer, bufSize);
                if (length < 0) {
                    std::cerr << "read error" << std::endl;
                    break;
                }
                int i = 0;
                while (i < length) {
                    inotify_event* event = (inotify_event*)&buffer[i];
                    if (event->len > 0) {
                        callback(event->name);
                    }
                    i += sizeof(inotify_event) + event->len;
                }
            }
            close(fd);
        }).detach();
    }
};
#endif