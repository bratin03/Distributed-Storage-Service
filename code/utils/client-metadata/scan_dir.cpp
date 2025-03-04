#include <iostream>
#include <filesystem>
#include <unordered_map>
#include <chrono>
#include <thread>

namespace fs = std::filesystem;

int main()
{
    // Change this to your target directory.
    std::string directory = "/home/bratin/Distributed-Storage-Service";

    // Data structure to track files: mapping path -> last modification time.
    std::unordered_map<std::string, fs::file_time_type> fileIndex;

    // Perform an initial scan of the directory and subdirectories.
    for (const auto &entry : fs::recursive_directory_iterator(directory))
    {
        if (fs::is_regular_file(entry))
        {
            fileIndex[entry.path().string()] = fs::last_write_time(entry);
        }
    }

    // Time interval (in seconds) between checks.
    const int interval = 1; // For example, every 5 seconds.

    while (true)
    {
        // Wait for T seconds.
        std::this_thread::sleep_for(std::chrono::seconds(interval));

        // Check for new or modified files.
        for (const auto &entry : fs::recursive_directory_iterator(directory))
        {
            if (fs::is_regular_file(entry))
            {
                std::string path = entry.path().string();
                auto currentTime = fs::last_write_time(entry);
                auto it = fileIndex.find(path);

                if (it == fileIndex.end())
                {
                    // New file detected.
                    std::cout << "New file: " << path << std::endl;
                    fileIndex[path] = currentTime;
                }
                else if (it->second != currentTime)
                {
                    // File was modified.
                    std::cout << "Modified file: " << path << std::endl;
                    it->second = currentTime;
                }
            }
        }

        // Check for deleted files: remove files that no longer exist.
        for (auto it = fileIndex.begin(); it != fileIndex.end();)
        {
            if (!fs::exists(it->first))
            {
                std::cout << "Deleted file: " << it->first << std::endl;
                it = fileIndex.erase(it);
            }
            else
            {
                ++it;
            }
        }
    }

    return 0;
}
