#include "cache.hpp"
#include <iostream>
#include <thread>
#include <chrono>

int main()
{
    // Create a cache with 5-second default TTL and 100 bytes max size
    cache::Cache myCache(std::chrono::seconds(5), 100);

    // Insert a few entries
    myCache.set("key1", "value1");
    myCache.set("key2", "value2");
    myCache.set("key3", "value3");

    // Access and print values
    std::cout << "key1: " << myCache.get("key1") << std::endl;
    std::cout << "key2: " << myCache.get("key2") << std::endl;

    // Wait for 6 seconds to let TTL expire
    std::this_thread::sleep_for(std::chrono::seconds(6));

    std::cout << "After TTL expiration:" << std::endl;
    std::cout << "key1: " << myCache.get("key1") << std::endl; // Should be empty
    std::cout << "key2: " << myCache.get("key2") << std::endl; // Should be empty

    // Insert more data to test LRU eviction
    myCache.set("a", std::string(40, 'A')); // 41 bytes
    myCache.set("b", std::string(40, 'B')); // 41 bytes
    myCache.set("c", std::string(40, 'C')); // Should evict "a" due to size limit

    std::cout << "Testing LRU eviction:" << std::endl;
    std::cout << "a: " << myCache.get("a") << std::endl; // Might be evicted
    std::cout << "b: " << myCache.get("b") << std::endl; // Should exist
    std::cout << "c: " << myCache.get("c") << std::endl; // Should exist

    return 0;
}
