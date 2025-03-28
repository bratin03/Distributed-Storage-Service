#include <iostream>
#include <string>
#include "dropbox_client.h"
#include <nlohmann/json.hpp>

int main(int argc, char *argv[])
{

    std::string folderPath = "";
    if (argc == 2)
    {
        // The folder path is provided as the first command-line argument.
        folderPath = argv[1];
    }
    // Initialize the Dropbox client using your config file.
    DropboxClient dropbox("config.json");

    // Get the folder listing.
    DropboxResponse response = dropbox.listContent(folderPath);

    // Check if the request was successful.
    if (response.responseCode != 200)
    {
        std::cerr << "Error retrieving folder listing: " << response.errorMessage << std::endl;
        return 1;
    }

    // Parse the content as JSON.
    nlohmann::json listing;
    try
    {
        listing = nlohmann::json::parse(response.content);
    }
    catch (const std::exception &e)
    {
        std::cerr << "Error parsing JSON response: " << e.what() << std::endl;
        return 1;
    }

    // Pretty-print the JSON result.
    std::cout << "Folder listing for " << folderPath << ":\n";
    std::cout << listing.dump(4) << std::endl;

    return 0;
}
