#include "Watcher.hpp"
#include <iostream>

// Constructor: Initializes the notifier to watch the specified path recursively,
// and sets up the event callback for the desired events.
FileWatcher::FileWatcher(const std::filesystem::path &pathToWatch,
                         std::shared_ptr<std::queue<FileEvent>> eventQueue)
    : m_pathToWatch(pathToWatch),
      m_eventQueue(eventQueue),
      m_running(false)
{
    // Specify only the events we care about.
    auto events = std::initializer_list<inotify::Event>{
        inotify::Event::create,
        inotify::Event::modify,
        inotify::Event::remove,
        inotify::Event::moved_from,
        inotify::Event::moved_to};

    // Build the notifier using inotify-cpp's builder.
    m_notifier = std::unique_ptr<inotify::NotifierBuilder>(new inotify::NotifierBuilder(
        inotify::BuildNotifier()
            .watchPathRecursively(m_pathToWatch)
            .onEvents(events, [this](inotify::Notification notification)
                      { this->handleNotification(notification); })
            // Optional: log unexpected events.
            .onUnexpectedEvent([](inotify::Notification notification) {

            })

            ));
}

// Destructor: stops the watcher if it is running.
FileWatcher::~FileWatcher()
{
    stop();
    if (m_notifierThread.joinable())
    {
        m_notifierThread.join();
    }
}

// Start the notifier loop on a separate thread.
void FileWatcher::start()
{
    if (m_running)
    {
        return;
    }
    m_running = true;
    m_notifierThread = std::thread([this]()
                                   { m_notifier->run(); });
}

// Stop the notifier and join the thread.
void FileWatcher::stop()
{
    if (!m_running)
    {
        return;
    }
    m_running = false;
    m_notifier->stop();
    if (m_notifierThread.joinable())
    {
        m_notifierThread.join();
    }
}

// Check if a file is hidden based on its filename.
bool FileWatcher::isHidden(const std::filesystem::path &filePath)
{
    std::string filename = filePath.filename().string();
    return !filename.empty() && filename[0] == '.';
}

// Callback invoked when an inotify event is detected.
void FileWatcher::handleNotification(inotify::Notification notification)
{
    // Skip hidden files.
    if (isHidden(notification.path))
    {
        return;
    }

    // Prepare a FileEvent and set the path.
    FileEvent fileEvent;
    fileEvent.path = notification.path;

    // Determine the event type.
    uint32_t eventMask = static_cast<uint32_t>(notification.event);
    if (eventMask & static_cast<uint32_t>(inotify::Event::create))
    {
        fileEvent.type = FileEventType::CREATE;
    }
    else if (eventMask & static_cast<uint32_t>(inotify::Event::modify))
    {
        fileEvent.type = FileEventType::MODIFY;
    }
    else if (eventMask & static_cast<uint32_t>(inotify::Event::remove))
    {
        fileEvent.type = FileEventType::REMOVE;
    }
    else if (eventMask & static_cast<uint32_t>(inotify::Event::moved_from))
    {
        fileEvent.type = FileEventType::MOVED_FROM;
    }
    else if (eventMask & static_cast<uint32_t>(inotify::Event::moved_to))
    {
        fileEvent.type = FileEventType::MOVED_TO;
    }
    else
    {
        // If event does not match one of our tracked types, ignore it.
        return;
    }

    // Push the event onto the queue.
    m_eventQueue->push(fileEvent);
}
