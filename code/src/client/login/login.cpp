#include "login.hpp"
#include "../load_config/load_config.hpp"

namespace login
{
    std::string signUpLoadBalancerip;
    unsigned short signUpLoadBalancerPort;
    std::string loginLoadBalancerip;
    unsigned short loginLoadBalancerPort;
    std::string metaLoadBalancerip;
    unsigned short metaLoadBalancerPort;
    std::string token;
    std::string username;
    std::string password;

    const std::string server_info_config_path = "config/server_config.json";

    void handle_server_info(json &server_info)
    {
        server_info = ConfigReader::load(server_info_config_path);
        signUpLoadBalancerip = ConfigReader::get_config_string("signup_ip", server_info);
        signUpLoadBalancerPort = ConfigReader::get_config_short("signup_port", server_info);
        loginLoadBalancerip = ConfigReader::get_config_string("login_ip", server_info);
        loginLoadBalancerPort = ConfigReader::get_config_short("login_port", server_info);
        metaLoadBalancerip = ConfigReader::get_config_string("metaserver_ip", server_info);
        metaLoadBalancerPort = ConfigReader::get_config_short("metaserver_port", server_info);
        serverUtils::notificationLoadBalancerip = ConfigReader::get_config_string("notification_ip", server_info);
        serverUtils::notificationLoadBalancerPort = ConfigReader::get_config_short("notification_port", server_info);

        MyLogger::info("SignUp LoadBalancer IP: " + signUpLoadBalancerip);
        MyLogger::info("SignUp LoadBalancer Port: " + std::to_string(signUpLoadBalancerPort));
        MyLogger::info("Login LoadBalancer IP: " + loginLoadBalancerip);
        MyLogger::info("Login LoadBalancer Port: " + std::to_string(loginLoadBalancerPort));
        MyLogger::info("Meta LoadBalancer IP: " + metaLoadBalancerip);
        MyLogger::info("Meta LoadBalancer Port: " + std::to_string(metaLoadBalancerPort));
    }

    void handle_user_info(int argc, char *argv[], json &user_info, const std::string &user_info_config_path)
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

    bool login()
    {
        json payload;
        httplib::Client cli(loginLoadBalancerip, loginLoadBalancerPort);
        MyLogger::info("Sending login request to " + loginLoadBalancerip + ":" + std::to_string(loginLoadBalancerPort));
        payload["userID"] = username;
        payload["password"] = password;
        auto res = cli.Post("/login", payload.dump(), "application/json");
        if (res)
        {
            if (res->status == 200)
            {
                MyLogger::info("Login successful: " + res->body);
                auto res_json = json::parse(res->body);
                token = res_json["token"];
                MyLogger::info("Token: " + token);
                return true;
            }
            else
            {
                MyLogger::error("Login failed (" + std::to_string(res->status) + "): " + res->body);
                std::cout << "Login failed (" << std::to_string(res->status) << "): " << res->body << std::endl;
                return false;
            }
        }
        else
        {
            MyLogger::error("Error: No response from server");
            std::cerr << "Error: No response from server" << std::endl;
            return false;
        }
    }

    json makeRequest(std::string &ip, unsigned short &port, const std::string &path, const json &payload)
    {
        // If token is empty, login first
        if (token.empty())
        {
            login();
            httplib::Client cli(ip, port);
            httplib::Headers headers = {
                {"Authorization", "Bearer " + token},
                {"Content-Type", "application/json"}};
            MyLogger::info("Sending request to " + ip + ":" + std::to_string(port) + path + "Payload: " + payload.dump());
            auto res = cli.Post(path.c_str(), headers, payload.dump(), "application/json");
            if (res)
            {
                if (res->status == 200)
                {
                    MyLogger::info("Request successful: " + res->body);
                    return json::parse(res->body); // Parse response body to JSON and return it
                }
                else
                {
                    MyLogger::error("Request failed (" + std::to_string(res->status) + "): " + res->body);
                    std::cout << "Request failed (" << std::to_string(res->status) << "): " << res->body << std::endl;
                    return nullptr; // Return nullptr if failed
                }
            }
            else
            {
                MyLogger::error("Error: No response from server");
                std::cerr << "Error: No response from server" << std::endl;
                return nullptr; // Return nullptr if no response from server
            }
        }

        // Try once with existing token
        httplib::Client cli(ip, port);
        httplib::Headers headers = {
            {"Authorization", "Bearer " + token},
            {"Content-Type", "application/json"}};
        MyLogger::info("Sending request to " + ip + ":" + std::to_string(port) + path + "Payload: " + payload.dump());
        auto res = cli.Post(path.c_str(), headers, payload.dump(), "application/json");
        if (res)
        {
            if (res->status == 200)
            {
                MyLogger::info("Request successful: " + res->body);
                return json::parse(res->body); // Parse response body to JSON and return it
            }
            else if (res->status == 403)
            {
                MyLogger::error("Request failed (" + std::to_string(res->status) + "): " + res->body);
                token.clear();
                return makeRequest(ip, port, path, payload); // Retry with new token
            }
        }
        else
        {
            MyLogger::error("Error: No response from server");
            std::cerr << "Error: No response from server" << std::endl;
            return nullptr; // Return nullptr if no response from server
        }
        return nullptr; // Return nullptr on other failures
    }

}