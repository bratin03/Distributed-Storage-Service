#include "cache.hpp"
#include <unordered_map>
#include <chrono>
#include <thread>
#include <mutex>
#include <list>

namespace cache
{

    class Cache::Impl
    {
    public:
        Impl(std::chrono::milliseconds defaultTTL, std::size_t maxSize)
            : defaultTTL_(defaultTTL), maxSize_(maxSize), currentSize_(0), stopCleaner_(false)
        {
            cleanerThread_ = std::thread(&Impl::cleaner, this);
        }

        ~Impl()
        {
            {
                std::lock_guard<std::mutex> lock(mutex_);
                stopCleaner_ = true;
            }
            if (cleanerThread_.joinable())
                cleanerThread_.join();
        }

        // Helper function to compute the total size of a vector of strings.
        std::size_t computeValueSize(const std::vector<std::string> &vec)
        {
            std::size_t total = 0;
            for (const auto &s : vec)
            {
                total += s.size();
            }
            return total;
        }

        // Insert or update a cache entry.
        void set(const std::string &key, const std::vector<std::string> &value,
                 std::chrono::milliseconds ttl)
        {
            std::lock_guard<std::mutex> lock(mutex_);

            // If the key exists already, remove it.
            auto found = cache_.find(key);
            if (found != cache_.end())
            {
                currentSize_ -= (key.size() + computeValueSize(found->second.entry.value));
                lru_.erase(found->second.lruIterator);
                cache_.erase(found);
            }

            // Calculate the expiration time.
            auto expireTime = std::chrono::steady_clock::now() + (ttl.count() > 0 ? ttl : defaultTTL_);
            std::size_t entrySize = key.size() + computeValueSize(value);

            // If a single entry exceeds the maximum cache size, skip caching.
            if (entrySize > maxSize_)
            {
                return;
            }

            // Evict least recently used items until there is enough space.
            while (currentSize_ + entrySize > maxSize_)
            {
                if (lru_.empty())
                    break; // Should not occur as entrySize < maxSize_.
                std::string oldKey = lru_.back();
                auto it = cache_.find(oldKey);
                if (it != cache_.end())
                {
                    currentSize_ -= (oldKey.size() + computeValueSize(it->second.entry.value));
                    lru_.pop_back();
                    cache_.erase(it);
                }
            }

            // Insert the new key at the front of the LRU list.
            lru_.push_front(key);
            CacheEntry entry{value, expireTime};
            CacheValue val{entry, lru_.begin()};
            cache_[key] = val;
            currentSize_ += entrySize;
        }

        // Retrieve a cache entry and update its LRU position.
        std::vector<std::string> get(const std::string &key)
        {
            std::lock_guard<std::mutex> lock(mutex_);
            auto it = cache_.find(key);
            if (it == cache_.end())
                return {};

            // Check if the entry is expired.
            auto now = std::chrono::steady_clock::now();
            if (now >= it->second.entry.expireTime)
            {
                currentSize_ -= (key.size() + computeValueSize(it->second.entry.value));
                lru_.erase(it->second.lruIterator);
                cache_.erase(it);
                return {};
            }
            // Update LRU order: move key to the front.
            lru_.erase(it->second.lruIterator);
            lru_.push_front(key);
            it->second.lruIterator = lru_.begin();
            return it->second.entry.value;
        }

    private:
        // Internal structure representing a cache entry with a vector of strings and its expiration time.
        struct CacheEntry
        {
            std::vector<std::string> value;
            std::chrono::steady_clock::time_point expireTime;
        };

        // Internal structure to hold a cache entry and its iterator in the LRU list.
        struct CacheValue
        {
            CacheEntry entry;
            std::list<std::string>::iterator lruIterator;
        };

        std::unordered_map<std::string, CacheValue> cache_;
        std::list<std::string> lru_; // Front: most recently used; back: least recently used.
        std::chrono::milliseconds defaultTTL_;
        std::size_t maxSize_;
        std::size_t currentSize_;
        std::mutex mutex_;
        bool stopCleaner_;
        std::thread cleanerThread_;

        // Background cleaner thread removes expired entries.
        void cleaner()
        {
            while (true)
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                std::lock_guard<std::mutex> lock(mutex_);
                if (stopCleaner_)
                    break;
                auto now = std::chrono::steady_clock::now();
                for (auto it = cache_.begin(); it != cache_.end();)
                {
                    if (now >= it->second.entry.expireTime)
                    {
                        currentSize_ -= (it->first.size() + computeValueSize(it->second.entry.value));
                        lru_.erase(it->second.lruIterator);
                        it = cache_.erase(it);
                    }
                    else
                    {
                        ++it;
                    }
                }
            }
        }
    };

    // Public interface implementations.
    Cache::Cache(std::chrono::milliseconds defaultTTL, std::size_t maxSize)
        : impl_(new Impl(defaultTTL, maxSize))
    {
    }

    Cache::~Cache()
    {
        delete impl_;
    }

    void Cache::set(const std::string &key, const std::vector<std::string> &value,
                    std::chrono::milliseconds ttl)
    {
        impl_->set(key, value, ttl);
    }

    std::vector<std::string> Cache::get(const std::string &key)
    {
        return impl_->get(key);
    }

} // namespace cache
