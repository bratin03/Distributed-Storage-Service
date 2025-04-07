#ifndef CACHE_HPP
#define CACHE_HPP

#include <string>
#include <chrono>

namespace cache
{

    class Cache
    {
    public:
        explicit Cache(std::chrono::milliseconds defaultTTL);
        ~Cache();

        void set(const std::string &key, const std::string &value,
                 std::chrono::milliseconds ttl = std::chrono::milliseconds(0));

        std::string get(const std::string &key);

    private:
        struct CacheEntry;
        void cleaner();

        class Impl;
        Impl *impl_;
    };

} // namespace cache

#endif