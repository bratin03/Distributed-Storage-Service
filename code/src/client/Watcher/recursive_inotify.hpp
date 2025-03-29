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
#include <unordered_map>
#include <utility>

namespace fs = std::filesystem;

// Define an enum for our inotify event types.
enum class InotifyEventType {
  Created,
  Deleted,
  Modified,
  MovedFrom,
  MovedTo,
  Other
};

// This function sets up a recursive inotify watcher and pushes events
// into the provided thread-safe queue.
// It will run until an error occurs (or you kill the thread).
inline void watch_directory(
    const std::string &root_dir,
    std::shared_ptr<std::queue<std::pair<InotifyEventType, std::string>>>
        eventQueue,
    std::shared_ptr<std::mutex> mtx) {
  // Initialize inotify (non-blocking mode).
  int inotify_fd = inotify_init1(IN_NONBLOCK);
  if (inotify_fd == -1) {
    perror("inotify_init1");
    return;
  }

  // Map to hold watch descriptors (wd) and their corresponding directory paths.
  std::unordered_map<int, std::string> wd_to_path;

  // Lambda to add a watch for a directory and update the map.
  auto add_watch = [&](const std::string &path) {
    int wd = inotify_add_watch(inotify_fd, path.c_str(),
                               IN_CREATE | IN_DELETE | IN_MODIFY |
                                   IN_MOVED_FROM | IN_MOVED_TO);
    if (wd == -1) {
      std::cerr << "Cannot watch " << path << "\n";
    } else {
      wd_to_path[wd] = path;
      // (Optional) You could log here that a watch was added.
    }
  };

  // Recursively add watches for the root directory and all its subdirectories.
  try {
    add_watch(root_dir);
    for (const auto &entry : fs::recursive_directory_iterator(root_dir)) {
      if (entry.is_directory()) {
        add_watch(entry.path().string());
      }
    }
  } catch (const fs::filesystem_error &e) {
    std::cerr << "Filesystem error: " << e.what() << "\n";
    close(inotify_fd);
    return;
  }

  // Buffer to read events; allocate enough space for several events.
  const size_t buf_len = 10 * (sizeof(struct inotify_event) + NAME_MAX + 1);
  char buffer[buf_len];

  while (true) {
    ssize_t length = read(inotify_fd, buffer, buf_len);
    if (length < 0) {
      if (errno == EAGAIN) {
        // No events available; sleep briefly then continue.
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        continue;
      } else {
        perror("read");
        break;
      }
    }

    // Process all events in the buffer.
    for (char *ptr = buffer; ptr < buffer + length;) {
      struct inotify_event *event =
          reinterpret_cast<struct inotify_event *>(ptr);
      std::string dir = wd_to_path[event->wd];
      std::string name = (event->len > 0) ? event->name : "";
      fs::path full_path = fs::path(dir) / name;

      // Determine the event type.
      InotifyEventType eventType;
      if (event->mask & IN_CREATE) {
        eventType = InotifyEventType::Created;
        // If a new directory is created, add a watch for it.
        if (event->mask & IN_ISDIR) {
          add_watch(full_path.string());
        }
      } else if (event->mask & IN_DELETE) {
        eventType = InotifyEventType::Deleted;
      } else if (event->mask & IN_MODIFY) {
        eventType = InotifyEventType::Modified;
      } else if (event->mask & IN_MOVED_FROM) {
        eventType = InotifyEventType::MovedFrom;
      } else if (event->mask & IN_MOVED_TO) {
        eventType = InotifyEventType::MovedTo;
        // If a directory is moved into the watched area, add a watch.
        if (event->mask & IN_ISDIR) {
          add_watch(full_path.string());
        }
      } else {
        eventType = InotifyEventType::Other;
      }

      // Push the event into the shared queue in a thread-safe manner.
      {
        std::lock_guard<std::mutex> lock(*mtx);
        eventQueue->push(std::make_pair(eventType, full_path.string()));
      }

      // Move to the next event in the buffer.
      ptr += sizeof(struct inotify_event) + event->len;
    }
  }

  close(inotify_fd);
}
