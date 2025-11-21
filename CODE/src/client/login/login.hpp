/*
    CS60002 - Distributed Systems
    Term Project - Spring 2025

    * Author 1: Bratin Mondal (21CS10016)
    * Author 2: Soukhin Nayek (21CS10062)
    * Author 3: Swarnabh Mandal (21CS10068)

    * Department of Computer Science and Engineering
    * Indian Institute of Technology, Kharagpur
*/

#ifndef LOGIN_HPP
#define LOGIN_HPP
#include <nlohmann/json.hpp>
#include <string>

#include "../../../utils/libraries/cpp-httplib/httplib.h"
#include "../serverUtils/serverUtils.hpp"

using json = nlohmann::json;
namespace login {
void handle_server_info(json &server_info);
void handle_user_info(int argc, char *argv[], json &user_info,
                      const std::string &user_info_config_path);
bool signup(const std::string &username, const std::string &password);
bool login();
json makeRequest(std::string &ip, unsigned short &port, const std::string &path, json &payload);
extern std::string signUpLoadBalancerip;
extern unsigned short signUpLoadBalancerPort;
extern std::string loginLoadBalancerip;
extern unsigned short loginLoadBalancerPort;
extern std::string metaLoadBalancerip;
extern unsigned short metaLoadBalancerPort;
extern std::string username;
extern std::string password;
extern std::string token;
}  // namespace login

#endif  // LOGIN_HPP