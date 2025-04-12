#include "serverUtils.hpp"

namespace serverUtils
{
    std::shared_ptr<cache::Cache> cache_instance = nullptr;
    std::string notificationLoadBalancerip;
    unsigned short notificationLoadBalancerPort;

    void initializeCache(std::chrono::milliseconds defaultTTL, std::size_t maxSize)
    {
        cache_instance = std::make_shared<cache::Cache>(defaultTTL, maxSize);
        MyLogger::info("Cache initialized with default TTL: " + std::to_string(defaultTTL.count()) + " ms and max size: " + std::to_string(maxSize) + " bytes");
    }

    json createFile(const std::string &file_key)
    {
        auto resp = login::makeRequest(login::metaLoadBalancerip, login::metaLoadBalancerPort, "/create-file", json{{"path", file_key}});
        if (resp == nullptr)
        {
            MyLogger::error("Failed to create file: " + file_key);
        }
        else
        {
            MyLogger::debug("Response from create file: " + resp.dump(4));
        }
        return resp;
    }

    json createDir(const std::string &dir_key)
    {
        auto resp = login::makeRequest(login::metaLoadBalancerip, login::metaLoadBalancerPort, "/create-directory", json{{"path", dir_key}});
        if (resp == nullptr)
        {
            MyLogger::error("Failed to create directory: " + dir_key);
        }
        else
        {
            MyLogger::debug("Response from create directory: " + resp.dump(4));
        }
        return resp;
    }
} // namespace serverUtils