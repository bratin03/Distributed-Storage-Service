#include <iostream>
#include <queue>
#include <thread>
#include <memory>
#include <string>
#include <chrono>
#include "dropbox_client.h"   // Assume this header defines DropboxClient and DropboxResponse.
#include <nlohmann/json.hpp>

// Function that monitors Dropbox events for a given file/folder and pushes relevant event fields onto a queue.
void monitorDropboxEvents(const std::string &filename,
                          std::shared_ptr<DropboxClient> dropboxClient,
                          std::shared_ptr<std::queue<nlohmann::json>> eventQueue)
{
    // Initial listing to obtain the cursor.
    DropboxResponse listResp = dropboxClient->listContent(filename);
    std::string cursor;
    try {
        if (listResp.metadata.contains("cursor")) {
            cursor = listResp.metadata["cursor"].get<std::string>();
            std::cout << "Initial cursor for " << filename << ": " << cursor << "\n";
        } else {
            std::cerr << "Cursor not found in initial listing for " << filename << "\n";
            return;
        }
    } catch (const std::exception &e) {
        std::cerr << "Error parsing initial metadata: " << e.what() << "\n";
        return;
    }

    const int timeout = 90; // seconds
    // Continuous long polling loop.
    while (true)
    {
        std::cout << "\nLongpolling for changes (timeout " << timeout << " seconds)...\n";
        DropboxResponse lpResp = dropboxClient->longpollFolder(cursor, timeout);
        bool changesDetected = false;
        try {
            if (lpResp.metadata.contains("changes"))
                changesDetected = lpResp.metadata["changes"].get<bool>();
        } catch (const std::exception &e) {
            std::cerr << "Error parsing longpoll metadata: " << e.what() << "\n";
        }

        if (changesDetected)
        {
            std::cout << "Changes detected. Fetching updates...\n";
            // Fetch detailed updates.
            DropboxResponse updatesResp = dropboxClient->continueListing(cursor);
            std::cout << "Fetched update events:\n" << updatesResp.content << "\n";
            
            // Process each entry to extract only the relevant fields.
            if (updatesResp.metadata.contains("entries") && updatesResp.metadata["entries"].is_array()) {
                for (auto& entry : updatesResp.metadata["entries"]) {
                    nlohmann::json eventItem;
                    
                    // Determine if it's a file or a folder.
                    std::string tag = entry[".tag"].get<std::string>();
                    if (tag == "file") {
                        eventItem["event_type"] = "update";  // Placeholder; you might use more specific logic.
                        eventItem["item_type"] = "file";
                        eventItem["full_path"] = entry["path_display"];
                        eventItem["rev"] = entry["rev"];
                        eventItem["content_hash"] = entry["content_hash"];
                        // Optional additional details.
                        eventItem["id"] = entry["id"];
                        eventItem["client_modified"] = entry["client_modified"];
                        eventItem["server_modified"] = entry["server_modified"];
                        eventItem["size"] = entry["size"];
                    } else if (tag == "folder") {
                        eventItem["event_type"] = "update";
                        eventItem["item_type"] = "folder";
                        eventItem["full_path"] = entry["path_display"];
                        eventItem["id"] = entry["id"];
                    }
                    
                    // Push the refined event onto the event queue.
                    eventQueue->push(eventItem);
                }
            }
            
            // Update the cursor for the next iteration.
            try {
                if (updatesResp.metadata.contains("cursor")) {
                    cursor = updatesResp.metadata["cursor"].get<std::string>();
                    std::cout << "Updated cursor: " << cursor << "\n";
                }
            } catch (const std::exception &e) {
                std::cerr << "Error updating cursor: " << e.what() << "\n";
            }
        }
        else
        {
            std::cout << "No changes detected.\n";
        }
        
        // Sleep briefly before the next longpoll call.
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}

int main(int argc, char* argv[])
{
    // Use a folder path provided as a command-line argument; default to "/Lordu".
    std::string folder = (argc > 1) ? argv[1] : "/Lordu";

    // Create a shared pointer to the Dropbox client.
    auto dropboxClient = std::make_shared<DropboxClient>("config.json");

    // Create a shared pointer to an event queue (using nlohmann::json to represent events).
    auto eventQueue = std::make_shared<std::queue<nlohmann::json>>();

    // Start the monitor function in a separate thread.
    std::thread monitorThread(monitorDropboxEvents, folder, dropboxClient, eventQueue);

    // Main loop: Process events from the event queue.
    while (true)
    {
        if (!eventQueue->empty())
        {
            // Retrieve the next event.
            nlohmann::json event = eventQueue->front();
            eventQueue->pop();
            std::cout << "Received event: " << event.dump() << std::endl;
        }
        // Sleep briefly before checking the queue again.
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    // (Note: In this example, the program runs indefinitely.
    // In a real application, you would have a way to signal the thread to exit and then join it.)
    monitorThread.join();
    return 0;
}
