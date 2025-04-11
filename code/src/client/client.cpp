#include <string>
#include "load_config/load_config.hpp"
#include "login/login.hpp" 

// Global constants
const std::string user_info_config_path = "config/user_info.json";
json user_info;
json server_info;


int main(int argc, char *argv[])
{
    login::handle_server_info(server_info);
    login::handle_user_info(argc, argv, user_info, user_info_config_path);
    login::login();
}