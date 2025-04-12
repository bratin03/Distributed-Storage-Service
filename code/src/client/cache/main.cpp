#include "cache.hpp"
#include <iostream>
#include <vector>
#include <chrono>
#include <thread>

int main()
{
    // Create a cache instance with a default TTL of 500ms and a maximum size (in bytes) of 100.
    cache::Cache myCache(std::chrono::milliseconds(500), 100);

    // Create a vector of strings to store as the value.
    std::vector<std::string> value = {"Hello", "World"};

    // Set the key "greeting" with the vector value and a custom TTL of 1000ms.
    myCache.set("greeting", value, std::chrono::milliseconds(1000));

    // Immediately retrieve and print the value.
    std::vector<std::string> result = myCache.get("greeting");
    if (!result.empty())
    {
        std::cout << "Cache hit: ";
        for (const auto &s : result)
        {
            std::cout << s << " ";
        }
        std::cout << std::endl;
    }
    else
    {
        std::cout << "Cache miss." << std::endl;
    }

    // Wait long enough to let the TTL expire.
    std::this_thread::sleep_for(std::chrono::milliseconds(1500));

    // Try to get the key "greeting" again after the TTL has expired.
    result = myCache.get("greeting");
    if (result.empty())
    {
        std::cout << "Cache entry expired." << std::endl;
    }
    else
    {
        std::cout << "Cache hit: ";
        for (const auto &s : result)
        {
            std::cout << s << " ";
        }
        std::cout << std::endl;
    }

    return 0;
}
