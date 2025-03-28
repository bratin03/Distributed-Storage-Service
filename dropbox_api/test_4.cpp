#include <iostream>
#include <string>
#include "dropbox_client.h"
#include <nlohmann/json.hpp>

int main(int argc, char* argv[])
{
    // Use a folder path provided as a command-line argument; default to "/large".
    std::string folder = "";
    if (argc > 1)
    {
        folder = argv[1];
    }

    // Initialize the Dropbox client.
    DropboxClient dropbox("config.json");

    // Step 1: List the folder to obtain the initial cursor.
    DropboxResponse listResp = dropbox.listContent(folder);
    std::cout << "Initial folder listing for " << folder << ":\n" << listResp.content << "\n";

    // Extract the cursor from the response.
    std::string cursor;
    try {
        if (listResp.metadata.contains("cursor"))
        {
            cursor = listResp.metadata["cursor"].get<std::string>();
            std::cout << "Extracted cursor: " << cursor << "\n";
        }
        else
        {
            std::cerr << "Cursor not found in initial folder listing response.\n";
            return 1;
        }
    } catch (const std::exception &e) {
        std::cerr << "Error parsing metadata: " << e.what() << "\n";
        return 1;
    }

    // Step 2: Longpoll for changes.
    int timeout = 30; // seconds
    std::cout << "Longpolling for changes (timeout " << timeout << " seconds)...\n";
    DropboxResponse lpResp = dropbox.longpollFolder(cursor, timeout);
    std::cout << "Longpoll response:\n" << lpResp.content << "\n";

    // Step 3: Check if changes were detected.
    bool changesDetected = false;
    try {
        if (lpResp.metadata.contains("changes"))
        {
            changesDetected = lpResp.metadata["changes"].get<bool>();
        }
    } catch (const std::exception &e) {
        std::cerr << "Error parsing longpoll response: " << e.what() << "\n";
    }

    if (changesDetected)
    {
        std::cout << "Changes detected. Fetching updates using the continue endpoint...\n";
        // Step 4: Call the continueListing() method to get the detailed updates.
        DropboxResponse updatesResp = dropbox.continueListing(cursor);
        std::cout << "Folder updates:\n" << updatesResp.content << "\n";
        // Note: The updated cursor is typically returned in updatesResp.metadata["cursor"]
        // which you can store for subsequent longpoll calls.
    }
    else
    {
        std::cout << "No changes detected.\n";
    }

    return 0;
}
