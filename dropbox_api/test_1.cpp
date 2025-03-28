#include <iostream>
#include <string>
#include <vector>
#include "dropbox_client.h"

#define N 5  // Number of directories
#define M 5  // Number of files per directory

// Helper function to generate content of a given size (filled with 'A's)
std::string generateContent(int size)
{
    return std::string(size, 'A');
}

// Helper function to print the full response for debugging.
void printResponse(const std::string &operation, const DropboxResponse &response)
{
    std::cout << "Operation: " << operation << "\n";
    std::cout << "Response Code: " << response.responseCode << "\n";
    if (!response.errorMessage.empty())
        std::cout << "Error Message: " << response.errorMessage << "\n";
    std::cout << "Content: " << response.content << "\n";
    std::cout << "Metadata: " << response.metadata.dump(4) << "\n";
    std::cout << "---------------------------\n";
}

int main()
{
    DropboxClient dropbox("config.json");
    std::vector<std::string> directories;
    std::vector<std::string> files;

    // Step 1: Create N directories
    for (int i = 1; i <= N; i++)
    {
        std::string dir = "/TestDir" + std::to_string(i);
        directories.push_back(dir);
        DropboxResponse response = dropbox.createFolder(dir);
        printResponse("Create folder " + dir, response);
    }

    // Step 2: Create M files in each directory (200 bytes each)
    for (const std::string &dir : directories)
    {
        for (int j = 1; j <= M; j++)
        {
            std::string filePath = dir + "/File" + std::to_string(j) + ".txt";
            files.push_back(filePath);
            DropboxResponse response = dropbox.createFile(filePath, generateContent(200));
            printResponse("Create file " + filePath, response);
        }
    }

    // Step 3: Modify files to contain 400 bytes, using file revision ("rev")
    for (const std::string &file : files)
    {
        // Use getMetadata to obtain file metadata (including "rev")
        DropboxResponse metaResponse = dropbox.getMetadata(file);
        if (metaResponse.responseCode == 200)
        {
            if (metaResponse.metadata.contains("rev") && metaResponse.metadata["rev"].is_string())
            {
                std::string rev = metaResponse.metadata["rev"].get<std::string>();
                DropboxResponse modResponse = dropbox.modifyFile(file, generateContent(400), rev);
                printResponse("Modify file " + file, modResponse);
            }
            else
            {
                std::cerr << "Error: File metadata for " << file << " does not contain a valid 'rev'. Skipping modification.\n";
            }
        }
        else
        {
            std::cerr << "Failed to get metadata for file " << file << "\n";
        }
    }

    // Step 4: Delete every second file (i.e. indices 0, 2, 4, ...)
    for (size_t i = 0; i < files.size(); i += 2)
    {
        DropboxResponse delResponse = dropbox.deleteFile(files[i]);
        printResponse("Delete file " + files[i], delResponse);
    }

    // Step 5: Print metadata for every third file (i.e. indices 1, 4, 7, ...)
    for (size_t i = 1; i < files.size(); i += 3)
    {
        DropboxResponse metaResponse = dropbox.getMetadata(files[i]);
        printResponse("Get metadata for " + files[i], metaResponse);
    }

    // Step 6: Delete every second directory
    for (size_t i = 0; i < directories.size(); i += 2)
    {
        DropboxResponse delResponse = dropbox.deleteFolder(directories[i]);
        printResponse("Delete directory " + directories[i], delResponse);
    }

    return 0;
}
