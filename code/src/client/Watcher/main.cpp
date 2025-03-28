#include <chrono>
#include <iostream>
#include <memory>
#include <queue>
#include <thread>
#include "Watcher.hpp"  // Make sure this includes the definition of FileEvent and FileWatcher

int main() {
    // Create a shared queue for file events.
    auto eventQueue = std::make_shared<std::queue<FileEvent>>();

    // Instantiate the FileWatcher for the desired directory.
    FileWatcher watcher("/home/bratin/RANDOM", eventQueue);
    
    // Start the file watcher in its own thread.
    watcher.start();

    // Create a polling thread to process events from the queue.
    std::thread pollingThread([eventQueue]() {
        while (true) {
            // Poll the queue periodically.
            while (!eventQueue->empty()) {
                FileEvent event = eventQueue->front();
                eventQueue->pop();

                // Process the event. For example, print event details:
                std::string eventType;
                switch (event.type) {
                    case FileEventType::CREATE:
                        eventType = "CREATE";
                        break;
                    case FileEventType::MODIFY:
                        eventType = "MODIFY";
                        break;
                    case FileEventType::REMOVE:
                        eventType = "REMOVE";
                        break;
                    case FileEventType::MOVED_FROM:
                        eventType = "MOVED_FROM";
                        break;
                    case FileEventType::MOVED_TO:
                        eventType = "MOVED_TO";
                        break;
                }
                std::cout << "Event: " << eventType 
                          << " on file: " << event.path << std::endl;
            }
            // Sleep briefly to avoid busy-waiting.
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    });

    // In a real application, you would have a mechanism to terminate the loops.
    // For this example, we'll just wait for the polling thread.
    pollingThread.join();

    // Optionally, stop the watcher (this example will not be reached unless you break the loop).
    watcher.stop();
    return 0;
}
