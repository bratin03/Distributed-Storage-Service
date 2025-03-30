#include <chrono>
#include <errno.h>
#include <filesystem>
#include <iostream>
#include <limits.h>
#include <memory>
#include <mutex>
#include <queue>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/inotify.h>
#include <thread>
#include <unistd.h>
#include <set>
#include <map>
#include <unordered_map>
#include <utility>
#include <condition_variable>

namespace fs = std::filesystem;

// Define an enum for our inotify event types.
enum class InotifyEventType
{
  Created,
  Deleted,
  Modified,
  MovedFrom,
  MovedTo,
  Other
};

// Define an enum for file type.
enum class FileType
{
  File,
  Directory,
  Unknown
};

// This structure holds an event with type, path, and file type.
struct FileEvent
{
  InotifyEventType eventType;
  std::string path;
  FileType fileType;
  // Add a comparison operator for the set.
  bool operator<(const FileEvent &other) const
  {
    return std::tie(eventType, path, fileType) <
           std::tie(other.eventType, other.path, other.fileType);
  }
};

inline void watch_directory(
    const std::string &root_dir,
    std::shared_ptr<std::queue<FileEvent>> eventQueue,
    std::shared_ptr<std::set<FileEvent>> eventMap,
    std::shared_ptr<std::mutex> mtx,
    std::shared_ptr<std::condition_variable> cv)
{
  // Initialize inotify (non-blocking mode).
  int inotify_fd = inotify_init1(IN_NONBLOCK);
  if (inotify_fd == -1)
  {
    perror("inotify_init1");
    return;
  }

  // Map to hold watch descriptors (wd) and their corresponding directory paths.
  std::unordered_map<int, std::string> wd_to_path;
  // Map to hold move events for directories: cookie -> old path.
  std::unordered_map<uint32_t, std::string> move_cookie_map;

  // Lambda to add a watch for a directory and update the map.
  auto add_watch = [&](const std::string &path)
  {
    int wd = inotify_add_watch(inotify_fd, path.c_str(),
                               IN_CREATE | IN_DELETE | IN_MODIFY |
                                   IN_MOVED_FROM | IN_MOVED_TO);
    if (wd == -1)
    {
      std::cerr << "Cannot watch " << path << "\n";
    }
    else
    {
      wd_to_path[wd] = path;
      // (Optional) Log that a watch was added.
    }
  };

  // Recursively add watches for the root directory and all its subdirectories.
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

  // Buffer to read events; allocate enough space for several events.
  const size_t buf_len = 10 * (sizeof(struct inotify_event) + NAME_MAX + 1);
  char buffer[buf_len];

  while (true)
  {
    ssize_t length = read(inotify_fd, buffer, buf_len);
    if (length < 0)
    {
      if (errno == EAGAIN)
      {
        // No events available; sleep briefly then continue.
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        continue;
      }
      else
      {
        perror("read");
        break;
      }
    }

    // Process all events in the buffer.
    for (char *ptr = buffer; ptr < buffer + length;)
    {
      struct inotify_event *event =
          reinterpret_cast<struct inotify_event *>(ptr);
      std::string dir = wd_to_path[event->wd];
      std::string name = (event->len > 0) ? event->name : "";
      fs::path full_path = fs::path(dir) / name;
      InotifyEventType eventType = InotifyEventType::Other;

      // Handle move events specially.
      if (event->mask & IN_MOVED_FROM)
      {
        eventType = InotifyEventType::MovedFrom;
        // For directories, save the old path using the cookie.
        if (event->mask & IN_ISDIR && event->cookie != 0)
        {
          move_cookie_map[event->cookie] = full_path.string();
        }
      }
      else if (event->mask & IN_MOVED_TO)
      {
        // Check if this MOVED_TO matches a previous MOVED_FROM for a directory.
        if ((event->mask & IN_ISDIR) && event->cookie != 0 &&
            move_cookie_map.find(event->cookie) != move_cookie_map.end())
        {
          // Compute new path.
          std::string old_path = move_cookie_map[event->cookie];
          // The new parent directory is given by wd_to_path[event->wd]
          std::string new_path = fs::path(wd_to_path[event->wd]) / name;

          // Update our wd_to_path mapping for the renamed directory and any descendant.
          for (auto &entry : wd_to_path)
          {
            // If the watched directory equals the old path, update it.
            if (entry.second == old_path)
            {
              entry.second = new_path;
            }
            // If it is a subdirectory (child) of the renamed directory, update its path.
            else if (entry.second.size() > old_path.size() &&
                     entry.second.compare(0, old_path.size(), old_path) == 0 &&
                     entry.second[old_path.size()] == fs::path::preferred_separator)
            {
              entry.second = new_path + entry.second.substr(old_path.size());
            }
          }

          // Report the MOVED_TO event for the renamed directory.
          {
            std::lock_guard<std::mutex> lock(*mtx);
            FileEvent fevent{InotifyEventType::MovedTo, new_path, FileType::Directory};
            if (eventMap->find(fevent) == eventMap->end())
            {
              eventQueue->push(fevent);
              eventMap->insert(fevent);
              cv->notify_one();
            }
          }

          // Recursively generate and report a MOVED_TO event for every file/subdirectory
          // inside the renamed directory.
          try
          {
            for (const auto &entry : fs::recursive_directory_iterator(new_path))
            {
              // The new full path is simply the path from the iterator.
              std::string child_new_path = entry.path().string();
              // Optionally, you could compute the old corresponding path if needed:
              // std::string relative = fs::relative(entry.path(), new_path).string();
              // std::string child_old_path = fs::path(old_path) / relative;
              FileType ft = entry.is_directory() ? FileType::Directory : FileType::File;
              FileEvent fevent{InotifyEventType::MovedTo, child_new_path, ft};
              std::lock_guard<std::mutex> lock(*mtx);
              if (eventMap->find(fevent) == eventMap->end())
              {
                eventQueue->push(fevent);
                eventMap->insert(fevent);
                cv->notify_one();
              }
            }
          }
          catch (const fs::filesystem_error &e)
          {
            std::cerr << "Filesystem error during recursive update: " << e.what() << "\n";
          }
          // Remove the cookie mapping now that it has been handled.
          move_cookie_map.erase(event->cookie);
          // Skip further processing of this MOVED_TO event.
          ptr += sizeof(struct inotify_event) + event->len;
          continue;
        }
        else
        {
          // For MOVED_TO events not part of a rename pair (or non-directory), process normally.
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
        // If a new directory is created, add a watch for it.
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

      // Check file type based on IN_ISDIR flag.
      FileType fileType = (event->mask & IN_ISDIR) ? FileType::Directory : FileType::File;

      // Push the FileEvent into the shared queue in a thread-safe manner.
      {
        std::lock_guard<std::mutex> lock(*mtx);
        FileEvent fevent{eventType, full_path.string(), fileType};
        // Check if the event is already in the map to avoid duplicates.
        if (eventMap->find(fevent) == eventMap->end())
        {
          eventQueue->push(fevent);
          eventMap->insert(fevent);
          cv->notify_one();
        }
      }

      // Move to the next event in the buffer.
      ptr += sizeof(struct inotify_event) + event->len;
    }
  }

  close(inotify_fd);
}
