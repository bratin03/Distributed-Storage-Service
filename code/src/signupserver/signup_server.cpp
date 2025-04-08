#include <iostream>
#include <fstream>
#include <string>
#include <sstream>
#include <nlohmann/json.hpp>
#define CPPHTTPLIB_HEADER_ONLY
#include "../../utils/libraries/cpp-httplib/httplib.h"
#include <hiredis/hiredis.h>
#include "logger/Mylogger.h"
#include "../../utils/Distributed_KV/client_lib/kv.hpp"

// Global
std::vector<std::vector<std::string>> blockserver_lists;

// For convenience
using json = nlohmann::json;

// Function to load configuration from JSON file
bool loadConfig(const std::string &filename, json &config)
{
    std::ifstream file(filename);
    if (!file.is_open())
    {
        MyLogger::error("Unable to open config file: " + filename);
        return false;
    }
    try
    {
        file >> config;
        MyLogger::info("Configuration loaded successfully from: " + filename);
    }
    catch (json::parse_error &e)
    {
        MyLogger::error("JSON parse error: " + std::string(e.what()));
        return false;
    }
    return true;
}

inline void load_blockserver_config(const std::string &filepath)
{
    std::ifstream file(filepath);
    if (!file.is_open())
    {
        MyLogger::error("Could not open blockserver_config.json");
        return;
    }

    json config_json;
    try
    {
        file >> config_json;
    }
    catch (const std::exception &e)
    {
        MyLogger::error("Failed to parse blockserver_config.json: " + std::string(e.what()));
        return;
    }

    blockserver_lists.clear();
    for (auto &[key, value] : config_json.items())
    {
        if (!value.is_array())
        {
            MyLogger::warning("Skipping malformed list in blockserver_config.json: " + key);
            continue;
        }

        std::vector<std::string> serverGroup;
        for (const auto &server : value)
        {
            if (server.is_string())
            {
                serverGroup.push_back(server.get<std::string>());
            }
            else
            {
                MyLogger::warning("Non-string element found in list: " + key);
            }
        }
        blockserver_lists.push_back(serverGroup);
    }

    MyLogger::info("Loaded blockserver config with " + std::to_string(blockserver_lists.size()) + " server groups.");
}

std::vector<std::string> &select_block_server_group(const std::string &key)
{
    static std::hash<std::string> hasher;
    size_t idx = hasher(key) % blockserver_lists.size();
    return blockserver_lists[idx];
}

// Function to perform atomic signup using Redis SET NX
bool signupUser(const std::string &username, const std::string &password, const std::string &redis_ip, int redis_port, std::string &errorMsg)
{
    // Connect to Redis
    redisContext *context = redisConnect(redis_ip.c_str(), redis_port);
    if (context == nullptr || context->err)
    {
        errorMsg = "Redis connection error: " + std::string(context ? context->errstr : "can't allocate redis context");
        MyLogger::error(errorMsg);
        if (context)
            redisFree(context);
        return false;
    }

    MyLogger::info("Connected to Redis at " + redis_ip + ":" + std::to_string(redis_port));

    // Prepare and execute the SET command with NX option for atomicity
    redisReply *reply = (redisReply *)redisCommand(context, "SET %s %s NX", username.c_str(), password.c_str());
    if (!reply)
    {
        errorMsg = "Redis command error.";
        MyLogger::error(errorMsg);
        redisFree(context);
        return false;
    }

    bool success = false;
    // If reply type is status and string is "OK", the key was set successfully
    if (reply->type == REDIS_REPLY_STATUS && std::string(reply->str) == "OK")
    {
        success = true;
        MyLogger::info("Signup successful for username: " + username);
    }
    else
    {
        // Otherwise, assume the username already exists
        errorMsg = "Username already exists or Redis returned an error.";
        MyLogger::warning(errorMsg);
    }

    if (success == true)
    {
        std::string RootKey = username + ":dropbox";
        auto servers = select_block_server_group(RootKey);
        json new_metadata = {
            {"owner", username},
            {"timestamp", std::chrono::system_clock::to_time_t(std::chrono::system_clock::now())},
            {"subdirectories", json::object()},
            {"files", json::object()}};
        auto val = new_metadata.dump();
        auto RootCreationStatus = distributed_KV::set(servers, RootKey, val);
        if (!RootCreationStatus.success)
        {
            MyLogger::error("Failed to create root directory metadata: " + RootCreationStatus.err);
            errorMsg = "Failed to create root directory metadata.";
            success = false;
        }
        else
        {
            MyLogger::info("Root directory created for user: " + username);
        }
    }

    freeReplyObject(reply);
    redisFree(context);
    return success;
}

int main(int argc, char *argv[])
{
    // Check command line arguments for config file path
    std::string config_path = "signup_config.json"; // Default path
    if (argc > 1)
    {
        config_path = argv[1];
        MyLogger::debug("Using config file specified in arguments: " + config_path);
    }
    else
    {
        MyLogger::warning("No config file specified, using default: " + config_path);
        MyLogger::info("Usage: " + std::string(argv[0]) + " [config_file_path]");
    }

    // Load the configuration file
    json config;
    if (!loadConfig(config_path, config))
    {
        MyLogger::error("Failed to load config from: " + config_path);
        return 1;
    }

    load_blockserver_config(config["blockserver_config_file"]);

    // Read HTTP server configuration
    std::string http_ip = config["http_server"]["ip"];
    int http_port = config["http_server"]["port"];

    // Read Redis configuration
    std::string redis_ip = config["redis"]["ip"];
    int redis_port = config["redis"]["port"];

    // Create an HTTP server using httplib
    httplib::Server server;

    // POST /signup endpoint
    server.Post("/signup", [redis_ip, redis_port](const httplib::Request &req, httplib::Response &res)
                {
                    try
                    {
                        // Parse the incoming JSON request body
                        auto req_json = json::parse(req.body);
                        if (!req_json.contains("username") || !req_json.contains("password"))
                        {
                            res.status = 400;
                            res.set_content("{\"error\": \"Missing username or password\"}", "application/json");
                            MyLogger::warning("Signup request missing username or password.");
                            return;
                        }

                        std::string username = req_json["username"];
                        std::string password = req_json["password"]; // In production, this should be a hashed password

                        std::string errorMsg;
                        if (signupUser(username, password, redis_ip, redis_port,errorMsg))
                        {
                            res.set_content("{\"status\": \"Signup successful\"}", "application/json");
                            MyLogger::info("Signup successful for user: " + username);
                        }
                        else
                        {
                            res.status = 409; // Conflict (username already exists)
                            res.set_content("{\"error\": \"" + errorMsg + "\"}", "application/json");
                            MyLogger::warning("Signup failed for user: " + username + ". Reason: " + errorMsg);
                        }
                    }
                    catch (json::parse_error &e)
                    {
                        res.status = 400;
                        res.set_content("{\"error\": \"Invalid JSON in request\"}", "application/json");
                        MyLogger::error("Invalid JSON in signup request. Error: " + std::string(e.what()));
                    }
                    catch (std::exception &e)
                    {
                        res.status = 500;
                        res.set_content("{\"error\": \"Server error\"}", "application/json");
                        MyLogger::error("Unexpected server error: " + std::string(e.what()));
                    } });

    MyLogger::info("Starting signup server on " + http_ip + ":" + std::to_string(http_port));

    server.listen(http_ip.c_str(), http_port);

    return 0;
}
