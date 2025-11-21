/*
    CS60002 - Distributed Systems
    Term Project - Spring 2025

    * Author 1: Bratin Mondal (21CS10016)
    * Author 2: Soukhin Nayek (21CS10062)
    * Author 3: Swarnabh Mandal (21CS10068)

    * Department of Computer Science and Engineering
    * Indian Institute of Technology, Kharagpur
*/

#ifndef CACHE_HPP
#define CACHE_HPP

#include <chrono>
#include <string>
#include <vector>

namespace cache {

class Cache {
   public:
    // Create a cache with a default time-to-live (TTL) and a maximum size.
    Cache(std::chrono::milliseconds defaultTTL, std::size_t maxSize);
    ~Cache();

    // Set a cache entry with a key and a vector of string values, with a TTL.
    // If ttl is 0, the default TTL is used.
    void set(const std::string &key, const std::vector<std::string> &value,
             std::chrono::milliseconds ttl = std::chrono::milliseconds(0));

    // Retrieve a cache entry.
    // Returns an empty vector if the key is not found or the entry is expired.
    std::vector<std::string> get(const std::string &key);

   private:
    class Impl;
    Impl *impl_;
};

}  // namespace cache

#endif  // CACHE_HPP
