#include <iostream>
#include "cache.hpp"
#include <thread>

int main()
{
    // Create a cache with default TTL of 2 seconds
    cache::Cache myCache(std::chrono::milliseconds(2000));

    myCache.set("hello", "world");
    std::cout << "Set key 'hello' = 'world'\n";

    std::string value = myCache.get("hello");
    std::cout << "Get key 'hello': " << (value.empty() ? "[empty]" : value) << "\n";

    std::this_thread::sleep_for(std::chrono::seconds(3)); // wait for key to expire

    value = myCache.get("hello");
    std::cout << "Get key 'hello' after expiration: " << (value.empty() ? "[empty]" : value) << "\n";

    return 0;
}
