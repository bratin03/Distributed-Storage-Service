#ifndef CACHE_HPP
#define CACHE_HPP

#include <string>
#include <chrono>
#include <cstddef>

namespace cache
{

    class Cache
    {
    public:
        // The constructor now takes a maximum cache size (in bytes).
        explicit Cache(std::chrono::milliseconds defaultTTL, std::size_t maxSize);
        ~Cache();

        // Insert/update a key/value pair with an optional TTL override.
        void set(const std::string &key, const std::string &value,
                 std::chrono::milliseconds ttl = std::chrono::milliseconds(0));

        // Get the value associated with a key; returns an empty string if not found or expired.
        std::string get(const std::string &key);

    private:
        struct CacheEntry;
        void cleaner();

        class Impl;
        Impl *impl_;
    };

} // namespace cache

#endif // CACHE_HPP
