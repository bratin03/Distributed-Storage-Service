#include <iostream>
#include "dropbox_client.h"

int main() {
    try {
        // Instantiate the Dropbox client with a path to your config.json.
        DropboxClient client("config.json");

        // Example 1: Create a file.
        std::cout << "Creating file..." << std::endl;
        DropboxResponse createResp = client.createFile("/example.txt", "I am Lord");
        std::cout << "Create File Response (" << createResp.responseCode << "): " << createResp.content << std::endl;

        // Example 2: List folder content.
        std::cout << "\nListing folder content..." << std::endl;
        DropboxResponse listResp = client.listContent("");
        std::cout << "List Content Response (" << listResp.responseCode << "): " << listResp.content << std::endl;

        // Example 3: Read file.
        std::cout << "\nReading file..." << std::endl;
        DropboxResponse readResp = client.readFile("/example.txt");
        std::cout << "Read File Response (" << readResp.responseCode << "): " << readResp.content << std::endl;

        // Example 4: Modify (overwrite) the file.
        std::cout << "\nModifying file..." << std::endl;
        DropboxResponse modifyResp = client.modifyFile("/example.txt", "Updated file content.");
        std::cout << "Modify File Response (" << modifyResp.responseCode << "): " << modifyResp.content << std::endl;

        // Example 5: Delete the file.
        // std::cout << "\nDeleting file..." << std::endl;
        // DropboxResponse deleteResp = client.deleteFile("/example.txt");
        // std::cout << "Delete File Response (" << deleteResp.responseCode << "): " << deleteResp.content << std::endl;
    } catch (const std::exception &e) {
        std::cerr << "Exception: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
