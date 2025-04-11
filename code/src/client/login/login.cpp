#include "login.hpp"
#include "../load_config/load_config.hpp"

namespace login
{
    std::string signUpLoadBalancerip;
    unsigned short signUpLoadBalancerPort;
    std::string loginLoadBalancerip;
    unsigned short loginLoadBalancerPort;

    const std::string server_info_config_path = "config/server_config.json";

    void handle_server_info(json &server_info)
    {
        server_info = ConfigReader::load(server_info_config_path);
        signUpLoadBalancerip = ConfigReader::get_config_string("signup_ip", server_info);
        signUpLoadBalancerPort = ConfigReader::get_config_short("signup_port", server_info);
        loginLoadBalancerip = ConfigReader::get_config_string("login_ip", server_info);
        loginLoadBalancerPort = ConfigReader::get_config_short("login_port", server_info);
        MyLogger::info("SignUp LoadBalancer IP: " + signUpLoadBalancerip);
        MyLogger::info("SignUp LoadBalancer Port: " + std::to_string(signUpLoadBalancerPort));
        MyLogger::info("Login LoadBalancer IP: " + loginLoadBalancerip);
        MyLogger::info("Login LoadBalancer Port: " + std::to_string(loginLoadBalancerPort));
    }

    void handle_user_info(int argc, char *argv[], json &user_info, std::string &username, std::string &password,
                          const std::string &user_info_config_path)
    {
        if (argc == 1)
        {
            user_info = ConfigReader::load(user_info_config_path);
            username = ConfigReader::get_config_string("username", user_info);
            if (username.empty())
            {
                MyLogger::error("No UserName has been Set");
                std::cout << "UserName has not been set" << std::endl;
                std::cout << "Usage: " << argv[0] << " <username> <password>" << std::endl;
                exit(1);
            }

            password = ConfigReader::get_config_string("password", user_info);
            if (password.empty())
            {
                MyLogger::error("No Password has been Set");
                std::cout << "Password has not been set" << std::endl;
                std::cout << "Usage: " << argv[0] << " <username> <password>" << std::endl;
                exit(1);
            }
        }
        else if (argc == 3)
        {
            username = argv[1];
            password = argv[2];
            user_info["username"] = username;
            user_info["password"] = password;

            if (!ConfigReader::save(user_info_config_path, user_info))
            {
                MyLogger::error("Failed to save user info");
                std::cout << "Failed to save user info" << std::endl;
                exit(1);
            }

            auto signup_result = login::signup(username, password);
            if (signup_result)
            {
                MyLogger::info("Signup successful");
                std::cout << "Signup successful" << std::endl;
            }
            else
            {
                MyLogger::error("Signup failed");
                std::cout << "Signup failed" << std::endl;
                exit(1);
            }
        }
        else
        {
            std::cout << "Usage: " << argv[0] << " [username] [password]" << std::endl;
            std::cout << "If no username and password are provided, the program will use the default values from config/user_info.json" << std::endl;
            exit(1);
        }
    }

    bool signup(const std::string &username, const std::string &password)
    {
        httplib::Client cli(signUpLoadBalancerip, signUpLoadBalancerPort);
        json payload;
        MyLogger::info("Sending signup request to " + signUpLoadBalancerip + ":" + std::to_string(signUpLoadBalancerPort));
        payload["username"] = username;
        payload["password"] = password;

        auto res = cli.Post("/signup", payload.dump(), "application/json");

        if (res)
        {
            if (res->status == 200)
            {
                MyLogger::info("Signup successful: " + res->body);
                return true;
            }
            else
            {
                MyLogger::error("Signup failed (" + std::to_string(res->status) + "): " + res->body);
                std::cout << "Signup failed (" << res->status << "): " << res->body << std::endl;
                return false;
            }
        }
        else
        {
            MyLogger::error("Error: No response from server");
            std::cerr << "Error: No response from server" << std::endl;
            return false;
        }
        return false;
    }

}