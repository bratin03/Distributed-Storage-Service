#ifndef WATCHER_HPP
#define WATCHER_HPP

#include <condition_variable>
#include <map>
#include <mutex>
#include <queue>
#include <set>
#include <string>

namespace watcher
{

    enum class InotifyEventType
    {
        Created,
        Deleted,
        Modified,
        MovedFrom,
        MovedTo,
        Other
    };

    enum class FileType
    {
        File,
        Directory,
        Unknown
    };

    struct FileEvent
    {
        InotifyEventType eventType;
        std::string path;
        FileType fileType;

        bool operator<(const FileEvent &other) const
        {
            return std::tie(eventType, path, fileType) <
                   std::tie(other.eventType, other.path, other.fileType);
        }
    };

    // Function to start watching a directory (blocking)
    void watch_directory(
        const std::string &root_dir,
        std::queue<FileEvent> &eventQueue,
        std::set<FileEvent> &eventMap,
        std::mutex &mtx,
        std::condition_variable &cv);

} // namespace watcher

#endif // WATCHER_HPP
