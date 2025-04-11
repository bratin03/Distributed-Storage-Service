#include <string>
#include "load_config/load_config.hpp"
#include "login/login.hpp"
#include "nlohmann/json.hpp"
#include "metadata/metadata.hpp"

// Global constants
const std::string user_info_config_path = "config/user_info.json";
const std::string user_config_path = "config/user_config.json";
json user_info;
json user_config;
json server_info;
std::shared_ptr<rocksdb::DB> db_instance = nullptr;

int main(int argc, char *argv[])
{
    login::handle_server_info(server_info);
    login::handle_user_info(argc, argv, user_info, user_info_config_path);
    user_info = ConfigReader::load(user_config_path);
    auto metadata_path = ConfigReader::get_config_string(user_info, "metadata_path");
    db_instance = std::make_shared<rocksdb::DB>(metadata_path);
    metadata::setDatabase(db_instance);
}