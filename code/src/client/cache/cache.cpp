#include "cache.hpp"
#include <unordered_map>
#include <chrono>
#include <thread>
#include <mutex>

namespace cache {

struct Cache::CacheEntry {
    std::string value;
    std::chrono::steady_clock::time_point expireTime;
};

class Cache::Impl {
public:
    Impl(std::chrono::milliseconds defaultTTL)
        : defaultTTL_(defaultTTL), stopCleaner_(false) {
        cleanerThread_ = std::thread(&Impl::cleaner, this);
    }

    ~Impl() {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            stopCleaner_ = true;
        }
        if (cleanerThread_.joinable())
            cleanerThread_.join();
    }

    void set(const std::string& key, const std::string& value,
             std::chrono::milliseconds ttl) {
        auto now = std::chrono::steady_clock::now();
        auto expireTime = now + (ttl.count() > 0 ? ttl : defaultTTL_);
        std::lock_guard<std::mutex> lock(mutex_);
        cache_[key] = { value, expireTime };
    }

    std::string get(const std::string& key) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = cache_.find(key);
        if (it != cache_.end()) {
            auto now = std::chrono::steady_clock::now();
            if (now < it->second.expireTime) {
                return it->second.value;
            } else {
                cache_.erase(it);
            }
        }
        return "";
    }

private:
    std::unordered_map<std::string, CacheEntry> cache_;
    std::chrono::milliseconds defaultTTL_;
    std::mutex mutex_;
    bool stopCleaner_;
    std::thread cleanerThread_;

    void cleaner() {
        while (true) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            std::lock_guard<std::mutex> lock(mutex_);
            if (stopCleaner_) break;
            auto now = std::chrono::steady_clock::now();
            for (auto it = cache_.begin(); it != cache_.end(); ) {
                if (now >= it->second.expireTime)
                    it = cache_.erase(it);
                else
                    ++it;
            }
        }
    }
};

// Cache public method definitions
Cache::Cache(std::chrono::milliseconds defaultTTL)
    : impl_(new Impl(defaultTTL)) {}

Cache::~Cache() {
    delete impl_;
}

void Cache::set(const std::string& key, const std::string& value,
                std::chrono::milliseconds ttl) {
    impl_->set(key, value, ttl);
}

std::string Cache::get(const std::string& key) {
    return impl_->get(key);
}

} // namespace cache
