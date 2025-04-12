#pragma once
#include <string>
#include <vector>
#include "../logger/Mylogger.h"
#include "../initiation/initiation.hpp"
#include "../../../utils/Distributed_KV/client_lib/kv.hpp"

namespace Database_handler
{

    std::vector<std::string> &select_metastorage_group(const std::string &key); // need to check this returning reference and taking the returned as reference

    std::vector<std::string> &select_block_server_group(const std::string &key);

    distributed_KV::Response get_directory_metadata(const std::string &key);

    distributed_KV::Response set_directory_metadata(const std::string &key, const json &metadata);

    distributed_KV::Response delete_directory_metadata(const std::string &key);

    distributed_KV::Response delete_blockdata(const std::string &key);

}
