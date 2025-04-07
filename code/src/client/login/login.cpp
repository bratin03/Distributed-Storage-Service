#include "login.hpp"
namespace login
{
    std::string signUpLoadBalancerip;
    unsigned short signUpLoadBalancerPort;
    std::string loginLoadBalancerip;
    unsigned short loginLoadBalancerPort;
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
        }
        else
        {
            std::cout << "Usage: " << argv[0] << " [username] [password]" << std::endl;
            std::cout << "If no username and password are provided, the program will use the default values from config/user_info.json" << std::endl;
            exit(1);
        }
    }

}