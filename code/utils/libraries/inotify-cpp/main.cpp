#include <chrono>
#include <filesystem>
#include <inotify-cpp/NotifierBuilder.h>
#include <iostream>
#include <sstream>
#include <thread>

using namespace inotify;

// Helper function to convert event bitmask into a string.
std::string eventToString(uint32_t eventMask)
{
    std::ostringstream oss;
    if (eventMask & static_cast<uint32_t>(Event::access))
        oss << "ACCESS ";
    if (eventMask & static_cast<uint32_t>(Event::modify))
        oss << "MODIFY ";
    if (eventMask & static_cast<uint32_t>(Event::attrib))
        oss << "ATTRIB ";
    if (eventMask & static_cast<uint32_t>(Event::close_write))
        oss << "CLOSE_WRITE ";
    if (eventMask & static_cast<uint32_t>(Event::close_nowrite))
        oss << "CLOSE_NOWRITE ";
    if (eventMask & static_cast<uint32_t>(Event::open))
        oss << "OPEN ";
    if (eventMask & static_cast<uint32_t>(Event::moved_from))
        oss << "MOVED_FROM ";
    if (eventMask & static_cast<uint32_t>(Event::moved_to))
        oss << "MOVED_TO ";
    if (eventMask & static_cast<uint32_t>(Event::create))
        oss << "CREATE ";
    if (eventMask & static_cast<uint32_t>(Event::remove))
        oss << "REMOVE ";
    if (eventMask & static_cast<uint32_t>(Event::move_self))
        oss << "MOVE_SELF ";
    return oss.str();
}

int main(int argc, char** argv)
{
    if (argc <= 1) {
        std::cout << "Usage: " << argv[0] << " /path/to/dir" << std::endl;
        return 1;
    }

    // Convert the directory argument to a std::filesystem::path.
    std::filesystem::path path(argv[1]);

    // Define the event handler that prints all event types.
    auto handleNotification = [&](Notification notification) {
        // Cast notification.event to uint32_t so our helper function can process it.
        std::string events = eventToString(static_cast<uint32_t>(notification.event));
        std::cout << "Event(s): " << events << "on " << notification.path << std::endl;
    };

    // Define an unexpected event handler (optional).
    auto handleUnexpectedNotification = [](Notification notification) {
        std::cout << "Unexpected event: "
                  << eventToString(static_cast<uint32_t>(notification.event)) << "on "
                  << notification.path << std::endl;
    };

    // Specify all the events you want to monitor.
    auto events
        = { Event::access,        Event::modify, Event::attrib,     Event::close_write,
            Event::close_nowrite, Event::open,   Event::moved_from, Event::moved_to, // For renames.
            Event::create,        Event::remove, Event::move_self };

    // Build the notifier:
    auto notifier = BuildNotifier()
                        .watchPathRecursively(path)
                        .onEvents(events, handleNotification)
                        .onUnexpectedEvent(handleUnexpectedNotification);

    // Start the notifier's event loop in a separate thread.
    std::thread notifierThread([&]() { notifier.run(); });

    // Run the event loop for 60 seconds.
    std::this_thread::sleep_for(std::chrono::seconds(60));
    notifier.stop();
    notifierThread.join();

    return 0;
}
