/*
    CS60002 - Distributed Systems
    Term Project - Spring 2025

    * Author 1: Bratin Mondal (21CS10016)
    * Author 2: Soukhin Nayek (21CS10062)
    * Author 3: Swarnabh Mandal (21CS10068)

    * Department of Computer Science and Engineering
    * Indian Institute of Technology, Kharagpur
*/

#pragma once
#include <string>
#include <vector>

#include "../../../utils/Distributed_KV/client_lib/kv.hpp"
#include "../initiation/initiation.hpp"
#include "Mylogger.hpp"

namespace Database_handler {

std::vector<std::string> &select_metastorage_group(
    const std::string
        &key);  // need to check this returning reference and taking the returned as reference

std::vector<std::string> &select_block_server_group(const std::string &key);

distributed_KV::Response get_directory_metadata(const std::string &key);

distributed_KV::Response set_directory_metadata(const std::string &key, const json &metadata);

distributed_KV::Response delete_directory_metadata(const std::string &key);

distributed_KV::Response delete_blockdata(const std::string &key);

}  // namespace Database_handler
