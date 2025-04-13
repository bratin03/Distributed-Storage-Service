#include "watcher.hpp"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <iostream>
#include <limits.h>
#include <string.h>
#include <sys/inotify.h>
#include <thread>
#include <unistd.h>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace fs = std::filesystem;

namespace watcher
{
    // Helper: returns true if 'path' is a descendant of 'base' (or equal).
    bool is_descendant(const std::string &base, const std::string &path)
    {
        if (base == path)
            return true;
        std::string baseWithSep = base;
        if (baseWithSep.back() != fs::path::preferred_separator)
            baseWithSep.push_back(fs::path::preferred_separator);
        return path.compare(0, baseWithSep.size(), baseWithSep) == 0;
    }

    // Initialize a snapshot of the directory tree under 'root_dir'.
    void init_snapshot(const std::string &root_dir,
                       std::unordered_set<std::string> &snapshot)
    {
        snapshot.insert(root_dir);
        for (const auto &entry : fs::recursive_directory_iterator(root_dir))
        {
            snapshot.insert(entry.path().string());
        }
    }

    // Returns all paths in the snapshot that are descendants (or equal) of 'base'.
    std::vector<std::string> get_snapshot_entries(const std::unordered_set<std::string> &snapshot,
                                                  const std::string &base)
    {
        std::vector<std::string> result;
        for (const auto &p : snapshot)
        {
            if (is_descendant(base, p))
                result.push_back(p);
        }
        return result;
    }

    // Returns a numeric "depth" for a path (the number of directory separators).
    size_t path_depth(const std::string &path)
    {
        return std::count(path.begin(), path.end(), fs::path::preferred_separator);
    }

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

        // Map watch descriptor to directory path.
        std::unordered_map<int, std::string> wd_to_path;
        // For a directory move, store the moved-from base directory per cookie.
        std::unordered_map<uint32_t, std::string> move_cookie_map;
        // Our internal snapshot of the current file/directory tree.
        std::unordered_set<std::string> snapshot;

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
                    add_watch(entry.path().string());
            }
        }
        catch (const fs::filesystem_error &e)
        {
            std::cerr << "Filesystem error: " << e.what() << "\n";
            close(inotify_fd);
            return;
        }

        try
        {
            init_snapshot(root_dir, snapshot);
        }
        catch (const fs::filesystem_error &e)
        {
            std::cerr << "Error initializing snapshot: " << e.what() << "\n";
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
                std::string pathStr = full_path.string();
                InotifyEventType eventType = InotifyEventType::Other;
                FileType fileType = (event->mask & IN_ISDIR) ? FileType::Directory : FileType::File;

                {
                    std::lock_guard<std::mutex> lock(mtx);

                    if (event->mask & IN_CREATE)
                    {
                        eventType = InotifyEventType::Created;
                        snapshot.insert(pathStr);
                        if (event->mask & IN_ISDIR)
                            add_watch(pathStr);
                    }
                    else if (event->mask & IN_DELETE)
                    {
                        eventType = InotifyEventType::Deleted;
                        if (event->mask & IN_ISDIR)
                        {
                            auto entries = get_snapshot_entries(snapshot, pathStr);
                            for (const auto &p : entries)
                            {
                                FileEvent fevent{InotifyEventType::Deleted, p,
                                                 (fs::is_directory(p) ? FileType::Directory : FileType::File)};
                                if (eventMap.find(fevent) == eventMap.end())
                                {
                                    eventQueue.push(fevent);
                                    eventMap.insert(fevent);
                                }
                                snapshot.erase(p);
                            }
                        }
                        else
                        {
                            snapshot.erase(pathStr);
                        }
                    }
                    else if (event->mask & IN_MODIFY)
                    {
                        eventType = InotifyEventType::Modified;
                    }
                    // --- MOVED_FROM handling ---
                    else if (event->mask & IN_MOVED_FROM)
                    {
                        eventType = InotifyEventType::MovedFrom;
                        if ((event->mask & IN_ISDIR) && event->cookie != 0)
                        {
                            // Retrieve all paths under the moved directory from the snapshot.
                            auto allEntries = get_snapshot_entries(snapshot, pathStr);
                            // Remove them from the snapshot.
                            for (const auto &p : allEntries)
                                snapshot.erase(p);

                            // Separate the base directory from its descendants.
                            std::vector<std::string> children;
                            bool baseReported = false;
                            for (const auto &p : allEntries)
                            {
                                if (p == pathStr)
                                    baseReported = true;
                                else
                                    children.push_back(p);
                            }
                            // Sort children so the deepest paths are reported first.
                            std::sort(children.begin(), children.end(),
                                      [](const std::string &a, const std::string &b)
                                      {
                                          return path_depth(a) > path_depth(b);
                                      });
                            // Issue moved-from events for children.
                            for (const auto &child : children)
                            {
                                FileEvent fevent{InotifyEventType::MovedFrom, child,
                                                 (fs::is_directory(child) ? FileType::Directory : FileType::File)};
                                if (eventMap.find(fevent) == eventMap.end())
                                {
                                    eventQueue.push(fevent);
                                    eventMap.insert(fevent);
                                }
                            }
                            // Issue the moved-from event for the base directory once.
                            if (baseReported)
                            {
                                FileEvent fevent{InotifyEventType::MovedFrom, pathStr,
                                                 FileType::Directory};
                                if (eventMap.find(fevent) == eventMap.end())
                                {
                                    eventQueue.push(fevent);
                                    eventMap.insert(fevent);
                                }
                            }
                            // Save the moved-from base directory using the event cookie.
                            move_cookie_map[event->cookie] = pathStr;
                        }
                        else
                        {
                            snapshot.erase(pathStr);
                            FileEvent fevent{InotifyEventType::MovedFrom, pathStr, fileType};
                            if (eventMap.find(fevent) == eventMap.end())
                            {
                                eventQueue.push(fevent);
                                eventMap.insert(fevent);
                            }
                        }
                    }
                    // --- MOVED_TO handling (using original logic) ---
                    else if (event->mask & IN_MOVED_TO)
                    {
                        if ((event->mask & IN_ISDIR) && event->cookie != 0 &&
                            move_cookie_map.find(event->cookie) != move_cookie_map.end())
                        {
                            // For a directory move, compute the new base as per the original logic.
                            std::string old_path = move_cookie_map[event->cookie];
                            std::string new_path = (fs::path(wd_to_path[event->wd]) / name).string();

                            // Update the watch mapping for the moved-from subtree.
                            for (auto &entry : wd_to_path)
                            {
                                if (entry.second == old_path)
                                    entry.second = new_path;
                                else if (entry.second.size() > old_path.size() &&
                                         entry.second.compare(0, old_path.size(), old_path) == 0 &&
                                         entry.second[old_path.size()] == fs::path::preferred_separator)
                                {
                                    entry.second = new_path + entry.second.substr(old_path.size());
                                }
                            }

                            // Report moved-to for the directory itself.
                            {
                                FileEvent fevent{InotifyEventType::MovedTo, new_path, FileType::Directory};
                                if (eventMap.find(fevent) == eventMap.end())
                                {
                                    eventQueue.push(fevent);
                                    eventMap.insert(fevent);
                                    cv.notify_one();
                                }
                            }

                            // Traverse the new directory to report moved-to for all descendants.
                            // Also insert these paths into the snapshot so future moved-from operations
                            // include the complete subtree.
                            try
                            {
                                for (const auto &entry : fs::recursive_directory_iterator(new_path))
                                {
                                    std::string child_new_path = entry.path().string();
                                    snapshot.insert(child_new_path); // update snapshot here
                                    FileType ft = entry.is_directory() ? FileType::Directory : FileType::File;
                                    FileEvent fevent{InotifyEventType::MovedTo, child_new_path, ft};
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
                            snapshot.insert(pathStr);
                            if (event->mask & IN_ISDIR)
                                add_watch(pathStr);
                        }
                    }

                    // Process any generic events (filter non-.txt files for FileType::File if desired)
                    if (!(fileType == FileType::File && fs::path(pathStr).extension() != ".txt"))
                    {
                        if (eventType != InotifyEventType::Other)
                        {
                            FileEvent fevent{eventType, pathStr, fileType};
                            if (eventMap.find(fevent) == eventMap.end())
                            {
                                eventQueue.push(fevent);
                                eventMap.insert(fevent);
                                cv.notify_one();
                            }
                        }
                    }
                } // end lock

                ptr += sizeof(struct inotify_event) + event->len;
            }
        }

        close(inotify_fd);
    }
} // namespace watcher
