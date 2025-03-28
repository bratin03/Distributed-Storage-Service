#include "recursive_inotify.hpp"  // or the corresponding header filename
#include <queue>
#include <mutex>
#include <memory>
#include <iostream>
#include <thread>

int main(int argc, char *argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <directory_to_watch>\n";
        return EXIT_FAILURE;
    }

    std::string dir_to_watch = argv[1];

    // Create shared resources for event queue and mutex.
    auto eventQueue = std::make_shared<std::queue<std::pair<InotifyEventType, std::string>>>();
    auto queueMutex = std::make_shared<std::mutex>();

    // Start the watcher in a separate thread.
    std::thread watcherThread(watch_directory, dir_to_watch, eventQueue, queueMutex);

    // In your main thread, process events as they arrive.
    while (true) {
        {
            std::lock_guard<std::mutex> lock(*queueMutex);
            while (!eventQueue->empty()) {
                auto [eventType, path] = eventQueue->front();
                eventQueue->pop();

                // Handle the event.
                std::cout << "Event: ";
                switch (eventType) {
                    case InotifyEventType::Created:
                        std::cout << "Created";
                        break;
                    case InotifyEventType::Deleted:
                        std::cout << "Deleted";
                        break;
                    case InotifyEventType::Modified:
                        std::cout << "Modified";
                        break;
                    case InotifyEventType::MovedFrom:
                        std::cout << "Moved From";
                        break;
                    case InotifyEventType::MovedTo:
                        std::cout << "Moved To";
                        break;
                    default:
                        std::cout << "Other";
                }
                std::cout << " - " << path << std::endl;
            }
        }
        // Sleep briefly before checking the queue again.
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }

    // (Unreachable in this example; in a real application, you'd join/stop appropriately.)
    watcherThread.join();
    return EXIT_SUCCESS;
}
