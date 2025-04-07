#ifndef LOGIN_HPP
#define LOGIN_HPP
#include <string>
#include "../load_config/load_config.hpp"

namespace login
{
    void handle_user_info(int argc, char *argv[], json &user_info, std::string &username, std::string &password,
                          const std::string &user_info_config_path);
    extern std::string signUpLoadBalancerip;
    extern unsigned short signUpLoadBalancerPort;
    extern std::string loginLoadBalancerip;
    extern unsigned short loginLoadBalancerPort;
}

#endif // LOGIN_HPP