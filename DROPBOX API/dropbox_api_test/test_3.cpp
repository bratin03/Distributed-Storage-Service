#include <iostream>
#include <string>
#include "dropbox_client.h"

int main(int argc, char* argv[])
{
    if (argc < 2)
    {
        std::cerr << "Usage: " << argv[0] << " <dropbox_file_path>" << std::endl;
        return 1;
    }
    
    // The file path to download is passed as a command-line argument.
    std::string filePath = argv[1];
    
    // Initialize Dropbox client using the configuration file.
    DropboxClient dropbox("config.json");
    
    // Download the file content.
    DropboxResponse response = dropbox.readFile(filePath);
    
    // Check if the request was successful.
    if (response.responseCode == 200)
    {
        std::cout << "Downloaded file content:\n" << response.content << std::endl;
    }
    else
    {
        std::cerr << "Failed to download file. Response Code: " << response.responseCode << std::endl;
        if (!response.errorMessage.empty())
            std::cerr << "Error Message: " << response.errorMessage << std::endl;
    }
    
    return 0;
}
