#include <iostream>
#include <string>
#include <vector>
#include "dropbox_client.h"
#include <nlohmann/json.hpp>

#define N 5  // Number of files directly in /large
#define M 2  // Number of subdirectories inside /large
#define K 2   // Number of files per subdirectory

// Helper function to generate content of a given size (filled with a specified character).
std::string generateContent(int size, char fillChar = 'B')
{
    return std::string(size, fillChar);
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

// Helper function to pretty print the directory listing from DropboxResponse metadata.
void printDirectoryListing(const nlohmann::json &metadata)
{
    if (metadata.contains("entries") && metadata["entries"].is_array())
    {
        std::cout << "Directory Listing:\n";
        for (const auto &entry : metadata["entries"])
        {
            std::string name = entry.value("name", "Unnamed");
            std::string tag = entry.value(".tag", "unknown");
            std::cout << " - " << name << " [" << tag << "]\n";
        }
        std::cout << "---------------------------\n";
    }
    else
    {
        std::cout << "No entries found in directory listing.\n";
    }
}

int main()
{
    DropboxClient dropbox("config.json");
    DropboxResponse response;
    std::vector<std::string> subDirectories;

    // Step 1: Create directory "/large"
    std::string largeDir = "/large";
    response = dropbox.createFolder(largeDir);
    printResponse("Create folder " + largeDir, response);

    // Step 2: Create N files directly under "/large"
    std::vector<std::string> largeFiles;
    for (int i = 1; i <= N; i++)
    {
        std::string filePath = largeDir + "/LargeFile" + std::to_string(i) + ".txt";
        largeFiles.push_back(filePath);
        response = dropbox.createFile(filePath, generateContent(200, 'B'));
        printResponse("Create file " + filePath, response);
    }

    // Step 3: Create M subdirectories inside "/large", each with K files.
    for (int i = 1; i <= M; i++)
    {
        std::string subDir = largeDir + "/SubDir" + std::to_string(i);
        subDirectories.push_back(subDir);
        response = dropbox.createFolder(subDir);
        printResponse("Create subfolder " + subDir, response);

        // Create K files inside each subdirectory.
        for (int j = 1; j <= K; j++)
        {
            std::string filePath = subDir + "/File" + std::to_string(j) + ".txt";
            response = dropbox.createFile(filePath, generateContent(100, 'C'));
            printResponse("Create file " + filePath, response);
        }
    }

    // Step 4: List the contents of "/large" and pretty print the directory listing.
    DropboxResponse listLarge = dropbox.listContent(largeDir);
    printResponse("List content of " + largeDir, listLarge);
    std::cout << "\nPretty Printed Directory Listing for " << largeDir << ":\n";
    printDirectoryListing(listLarge.metadata);

    // Step 5: List the contents of one of the subdirectories (e.g., the first one) and pretty print it.
    if (!subDirectories.empty())
    {
        DropboxResponse listSub = dropbox.listContent(subDirectories[0]);
        printResponse("List content of " + subDirectories[0], listSub);
        std::cout << "\nPretty Printed Directory Listing for " << subDirectories[0] << ":\n";
        printDirectoryListing(listSub.metadata);
    }

    return 0;
}
