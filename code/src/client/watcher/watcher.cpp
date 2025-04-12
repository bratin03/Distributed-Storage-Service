#include "watcher.hpp"

#include <chrono>
#include <filesystem>
#include <iostream>
#include <limits.h>
#include <string.h>
#include <sys/inotify.h>
#include <thread>
#include <unistd.h>
#include <unordered_map>

namespace fs = std::filesystem;

namespace watcher
{

    void watch_directory(
        const std::string &root_dir,
        std::queue<FileEvent> &eventQueue,
        std::set<FileEvent> &eventMap,
        std::mutex &mtx,
        std::condition_variable &cv)
    {
        int inotify_fd = inotify_init1(IN_NONBLOCK);
        if (inotify_fd == -1)
        {
            perror("inotify_init1");
            return;
        }

        std::unordered_map<int, std::string> wd_to_path;
        std::unordered_map<uint32_t, std::string> move_cookie_map;

        auto add_watch = [&](const std::string &path)
        {
            int wd = inotify_add_watch(inotify_fd, path.c_str(),
                                       IN_CREATE | IN_DELETE | IN_MODIFY | IN_MOVED_FROM | IN_MOVED_TO);
            if (wd != -1)
            {
                wd_to_path[wd] = path;
            }
        };

        try
        {
            add_watch(root_dir);
            for (const auto &entry : fs::recursive_directory_iterator(root_dir))
            {
                if (entry.is_directory())
                {
                    add_watch(entry.path().string());
                }
            }
        }
        catch (const fs::filesystem_error &e)
        {
            std::cerr << "Filesystem error: " << e.what() << "\n";
            close(inotify_fd);
            return;
        }

        const size_t buf_len = 10 * (sizeof(struct inotify_event) + NAME_MAX + 1);
        char buffer[buf_len];

        while (true)
        {
            ssize_t length = read(inotify_fd, buffer, buf_len);
            if (length < 0)
            {
                if (errno == EAGAIN)
                {
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                    continue;
                }
                else
                {
                    perror("read");
                    break;
                }
            }

            for (char *ptr = buffer; ptr < buffer + length;)
            {
                struct inotify_event *event = reinterpret_cast<struct inotify_event *>(ptr);
                std::string dir = wd_to_path[event->wd];
                std::string name = (event->len > 0) ? event->name : "";
                fs::path full_path = fs::path(dir) / name;
                InotifyEventType eventType = InotifyEventType::Other;

                if (event->mask & IN_MOVED_FROM)
                {
                    eventType = InotifyEventType::MovedFrom;
                    if (event->mask & IN_ISDIR && event->cookie != 0)
                    {
                        move_cookie_map[event->cookie] = full_path.string();
                    }
                }
                else if (event->mask & IN_MOVED_TO)
                {
                    if ((event->mask & IN_ISDIR) && event->cookie != 0 &&
                        move_cookie_map.find(event->cookie) != move_cookie_map.end())
                    {
                        std::string old_path = move_cookie_map[event->cookie];
                        std::string new_path = (fs::path(wd_to_path[event->wd]) / name).string();

                        for (auto &entry : wd_to_path)
                        {
                            if (entry.second == old_path)
                            {
                                entry.second = new_path;
                            }
                            else if (entry.second.size() > old_path.size() &&
                                     entry.second.compare(0, old_path.size(), old_path) == 0 &&
                                     entry.second[old_path.size()] == fs::path::preferred_separator)
                            {
                                entry.second = new_path + entry.second.substr(old_path.size());
                            }
                        }

                        {
                            std::lock_guard<std::mutex> lock(mtx);
                            FileEvent fevent{InotifyEventType::MovedTo, new_path, FileType::Directory};
                            if (eventMap.find(fevent) == eventMap.end())
                            {
                                eventQueue.push(fevent);
                                eventMap.insert(fevent);
                                cv.notify_one();
                            }
                        }

                        try
                        {
                            for (const auto &entry : fs::recursive_directory_iterator(new_path))
                            {
                                std::string child_new_path = entry.path().string();
                                FileType ft = entry.is_directory() ? FileType::Directory : FileType::File;
                                FileEvent fevent{InotifyEventType::MovedTo, child_new_path, ft};
                                std::lock_guard<std::mutex> lock(mtx);
                                if (eventMap.find(fevent) == eventMap.end())
                                {
                                    eventQueue.push(fevent);
                                    eventMap.insert(fevent);
                                    cv.notify_one();
                                }
                            }
                        }
                        catch (const fs::filesystem_error &e)
                        {
                            std::cerr << "Filesystem error during recursive update: " << e.what() << "\n";
                        }

                        move_cookie_map.erase(event->cookie);
                        ptr += sizeof(struct inotify_event) + event->len;
                        continue;
                    }
                    else
                    {
                        eventType = InotifyEventType::MovedTo;
                        if (event->mask & IN_ISDIR)
                        {
                            add_watch(full_path.string());
                        }
                    }
                }
                else if (event->mask & IN_CREATE)
                {
                    eventType = InotifyEventType::Created;
                    if (event->mask & IN_ISDIR)
                    {
                        add_watch(full_path.string());
                    }
                }
                else if (event->mask & IN_DELETE)
                {
                    eventType = InotifyEventType::Deleted;
                }
                else if (event->mask & IN_MODIFY)
                {
                    eventType = InotifyEventType::Modified;
                }

                FileType fileType = (event->mask & IN_ISDIR) ? FileType::Directory : FileType::File;

                if (!(fileType == FileType::File && full_path.extension() != ".txt"))
                {
                    std::lock_guard<std::mutex> lock(mtx);
                    FileEvent fevent{eventType, full_path.string(), fileType};
                    if (eventMap.find(fevent) == eventMap.end())
                    {
                        eventQueue.push(fevent);
                        eventMap.insert(fevent);
                        cv.notify_one();
                    }
                }

                ptr += sizeof(struct inotify_event) + event->len;
            }
        }

        close(inotify_fd);
    }

} // namespace watcher
