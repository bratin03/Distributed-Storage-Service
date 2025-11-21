/*
    CS60002 - Distributed Systems
    Term Project - Spring 2025

    * Author 1: Bratin Mondal (21CS10016)
    * Author 2: Soukhin Nayek (21CS10062)
    * Author 3: Swarnabh Mandal (21CS10068)

    * Department of Computer Science and Engineering
    * Indian Institute of Technology, Kharagpur
*/

#include "cache.hpp"

#include <chrono>
#include <list>
#include <mutex>
#include <thread>
#include <unordered_map>

namespace cache {

class Cache::Impl {
   public:
    Impl(std::chrono::milliseconds defaultTTL, std::size_t maxSize)
        : _defaultTTL(defaultTTL), _maxSize(maxSize), _currentSize(0), _stopCleaner(false) {
        _cleanerThread = std::thread(&Impl::cleaner, this);
    }

    ~Impl() {
        {
            std::lock_guard<std::mutex> lock(_mutex);
            _stopCleaner = true;
        }
        if (_cleanerThread.joinable()) _cleanerThread.join();
    }

    // Helper function to compute the total size of a vector of strings.
    std::size_t computeValueSize(const std::vector<std::string> &vec) {
        std::size_t total = 0;
        for (const auto &s : vec) {
            total += s.size();
        }
        return total;
    }

    // Insert or update a cache entry.
    void set(const std::string &key, const std::vector<std::string> &value,
             std::chrono::milliseconds ttl) {
        std::lock_guard<std::mutex> lock(_mutex);

        // If the key exists already, remove it.
        auto found = _cache.find(key);
        if (found != _cache.end()) {
            _currentSize -= (key.size() + computeValueSize(found->second.entry.value));
            _lru.erase(found->second.lruIterator);
            _cache.erase(found);
        }

        // Calculate the expiration time.
        auto expireTime = std::chrono::steady_clock::now() + (ttl.count() > 0 ? ttl : _defaultTTL);
        std::size_t entrySize = key.size() + computeValueSize(value);

        // If a single entry exceeds the maximum cache size, skip caching.
        if (entrySize > _maxSize) {
            return;
        }

        // Evict least recently used items until there is enough space.
        while (_currentSize + entrySize > _maxSize) {
            if (_lru.empty()) break;  // Should not occur as entrySize < _maxSize.
            std::string oldKey = _lru.back();
            auto it = _cache.find(oldKey);
            if (it != _cache.end()) {
                _currentSize -= (oldKey.size() + computeValueSize(it->second.entry.value));
                _lru.pop_back();
                _cache.erase(it);
            }
        }

        // Insert the new key at the front of the LRU list.
        _lru.push_front(key);
        CacheEntry entry{value, expireTime};
        CacheValue val{entry, _lru.begin()};
        _cache[key] = val;
        _currentSize += entrySize;
    }

    // Retrieve a cache entry and update its LRU position.
    std::vector<std::string> get(const std::string &key) {
        std::lock_guard<std::mutex> lock(_mutex);
        auto it = _cache.find(key);
        if (it == _cache.end()) return {};

        // Check if the entry is expired.
        auto now = std::chrono::steady_clock::now();
        if (now >= it->second.entry.expireTime) {
            _currentSize -= (key.size() + computeValueSize(it->second.entry.value));
            _lru.erase(it->second.lruIterator);
            _cache.erase(it);
            return {};
        }
        // Update LRU order: move key to the front.
        _lru.erase(it->second.lruIterator);
        _lru.push_front(key);
        it->second.lruIterator = _lru.begin();
        return it->second.entry.value;
    }

   private:
    // Internal structure representing a cache entry with a vector of strings and its expiration
    // time.
    struct CacheEntry {
        std::vector<std::string> value;
        std::chrono::steady_clock::time_point expireTime;
    };

    // Internal structure to hold a cache entry and its iterator in the LRU list.
    struct CacheValue {
        CacheEntry entry;
        std::list<std::string>::iterator lruIterator;
    };

    std::unordered_map<std::string, CacheValue> _cache;
    std::list<std::string> _lru;  // Front: most recently used; back: least recently used.
    std::chrono::milliseconds _defaultTTL;
    std::size_t _maxSize;
    std::size_t _currentSize;
    std::mutex _mutex;
    bool _stopCleaner;
    std::thread _cleanerThread;

    // Background cleaner thread removes expired entries.
    void cleaner() {
        while (true) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            std::lock_guard<std::mutex> lock(_mutex);
            if (_stopCleaner) break;
            auto now = std::chrono::steady_clock::now();
            for (auto it = _cache.begin(); it != _cache.end();) {
                if (now >= it->second.entry.expireTime) {
                    _currentSize -= (it->first.size() + computeValueSize(it->second.entry.value));
                    _lru.erase(it->second.lruIterator);
                    it = _cache.erase(it);
                } else {
                    ++it;
                }
            }
        }
    }
};

// Public interface implementations.
Cache::Cache(std::chrono::milliseconds defaultTTL, std::size_t maxSize)
    : impl_(new Impl(defaultTTL, maxSize)) {}

Cache::~Cache() {
    delete impl_;
}

void Cache::set(const std::string &key, const std::vector<std::string> &value,
                std::chrono::milliseconds ttl) {
    impl_->set(key, value, ttl);
}

std::vector<std::string> Cache::get(const std::string &key) {
    return impl_->get(key);
}

}  // namespace cache
