#include "database_handler.hpp"


namespace Database_handler
{

    std::vector<std::string> &select_metastorage_group(const std::string &key)
    {
        static std::hash<std::string> hasher;
        size_t idx = hasher(key) % Initiation::metastorage_groups.size();
        return Initiation::metastorage_groups[idx];
    }

    std::vector<std::string> &select_block_server_group(const std::string &key)
    {
        static std::hash<std::string> hasher;
        size_t idx = hasher(key) % Initiation::blockserver_lists.size();
        return Initiation::blockserver_lists[idx];
    }

    distributed_KV::Response get_directory_metadata(const std::string &key)
    {
        auto &servers = select_metastorage_group(key);
        distributed_KV::Response res = distributed_KV::get(servers, key);

        if (!res.success)
        {
            MyLogger::warning("KV GET failed for key: " + key + " | Error: " + res.err);
            return res;
        }

        return res;
    }

    distributed_KV::Response set_directory_metadata(const std::string &key, const json &metadata)
    {
        std::string value = metadata.dump();
        return distributed_KV::set(select_metastorage_group(key), key, value);
    }

    distributed_KV::Response delete_directory_metadata(const std::string &key)
    {
        return distributed_KV::del(select_metastorage_group(key), key);
    }

    distributed_KV::Response delete_blockdata(const std::string &key)
    {
        return distributed_KV::del(select_block_server_group(key), key);
    }
    

}