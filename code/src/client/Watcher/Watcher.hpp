#ifndef FILEWATCHER_H
#define FILEWATCHER_H

#include <inotify-cpp/NotifierBuilder.h>
#include <filesystem>
#include <memory>
#include <queue>
#include <string>
#include <thread>

// Enum to represent the supported file event types.
enum class FileEventType {
    CREATE,
    MODIFY,
    REMOVE,
    MOVED_FROM,
    MOVED_TO
};

// Struct to encapsulate a file event.
struct FileEvent {
    FileEventType type;
    std::filesystem::path path;
};

class FileWatcher {
public:
    // Constructor takes a directory path (recursive watch) and a shared pointer to a queue
    // where FileEvent objects will be pushed.
    FileWatcher(const std::filesystem::path& pathToWatch,
                std::shared_ptr<std::queue<FileEvent>> eventQueue);
    ~FileWatcher();

    // Starts the watcher in a separate thread.
    void start();

    // Stops the watcher.
    void stop();

private:
    // Callback for inotify notifications.
    void handleNotification(inotify::Notification notification);

    // Helper to determine if a file is hidden (its filename starts with a dot).
    bool isHidden(const std::filesystem::path& filePath);

    std::filesystem::path m_pathToWatch;
    std::shared_ptr<std::queue<FileEvent>> m_eventQueue;
    std::unique_ptr<inotify::NotifierBuilder> m_notifier;
    std::thread m_notifierThread;
    bool m_running;
};

#endif // FILEWATCHER_H
