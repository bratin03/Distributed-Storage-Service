#include "../../utils/libraries/cpp-httplib/httplib.h"
#include "../../utils/libraries/jwt-cpp/include/jwt-cpp/jwt.h"
#include <nlohmann/json.hpp>
#include "./logger/Mylogger.h"

#include <hiredis/hiredis.h> // Hiredis client

#include <iostream>
#include <fstream>
#include <string>
#include <unordered_map>
#include <memory>

using json = nlohmann::json;
using namespace httplib;

// Global Redis context and private key
redisContext *redis_ctx = nullptr;
std::string PRIVATE_KEY;
std::string server_ip;
int server_port;
// Load RSA Private Key
std::string loadKey(const std::string &filename)
{
    MyLogger::info("Loading private key from file: " + filename);
    std::ifstream file(filename, std::ios::in);

    if (!file.is_open())
    {
        throw std::runtime_error("Failed to open key file: " + filename);
    }

    std::string key((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    if (key.empty())
    {
        throw std::runtime_error("Key file is empty: " + filename);
    }

    return key;
}

// Load config from file
json loadConfig(const std::string &path)
{
    std::ifstream file(path);
    if (!file.is_open())
    {
        throw std::runtime_error("Could not open config file: " + path);
    }
    return json::parse(file);
}

// Initialize hiredis connection
void initRedis(const std::string &host, int port)
{
    redis_ctx = redisConnect(host.c_str(), port);
    if (redis_ctx == nullptr || redis_ctx->err)
    {
        std::string err = redis_ctx ? redis_ctx->errstr : "Connection failed";
        throw std::runtime_error("Failed to connect to Redis: " + err);
    }

    MyLogger::info("Connected to Redis at " + host + ":" + std::to_string(port));
}

// Generate JWT token
std::string generateJWT(const std::string &userID)
{
    MyLogger::info("Generating JWT token");

    auto token = jwt::create()
                     .set_issuer("auth-server")
                     .set_type("JWT")
                     .set_subject(userID)
                     .set_payload_claim("userID", jwt::claim(userID))
                     .set_expires_at(std::chrono::system_clock::now() + std::chrono::minutes(30))
                     .sign(jwt::algorithm::rs256("", PRIVATE_KEY, "", ""));

    return token;
}

// Authenticate user using hiredis
json authenticateUser(const std::string &userID, const std::string &password)
{
    if (!redis_ctx)
    {
        return {{"status", "error"}, {"message", "Redis not initialized"}};
    }

    redisReply *reply = (redisReply *)redisCommand(redis_ctx, "GET %s", userID.c_str());
    if (!reply)
    {
        MyLogger::error("Redis command failed");
        return {{"status", "error"}, {"message", "Redis command error"}};
    }

    json result;
    if (reply->type == REDIS_REPLY_STRING && reply->str == password)
    {
        MyLogger::debug("Valid user from Redis");
        result = {{"status", "success"}, {"token", generateJWT(userID)}};
    }
    else
    {
        MyLogger::debug("Invalid user from Redis");
        result = {{"status", "error"}, {"message", "Invalid credentials"}};
    }

    freeReplyObject(reply);
    return result;
}

int main()
{
 
    try
    {
        // Load config
        json config = loadConfig("auth_config.json");
        server_ip = config["server_ip"];
        server_port = config["server_port"];

        std::string redis_host = config["redis_host"];
        int redis_port = config["redis_port"];
        std::string key_path = config["private_key_path"];

        // Init Redis and load private key
        initRedis(redis_host, redis_port);
        PRIVATE_KEY = loadKey(key_path);
    }
    catch (const std::exception &e)
    {
        MyLogger::error("Initialization failed: " + std::string(e.what()));
        return 1;
    }

    httplib::Server svr;

    svr.Post("/login", [](const httplib::Request &req, httplib::Response &res)
             {
        MyLogger::info("Received login request");

        try {
            auto body = json::parse(req.body);
            if (!body.contains("userID") || !body.contains("password")) {
                res.status = 400;
                res.set_content(R"({"status": "error", "message": "Invalid JSON format"})", "application/json");
                return;
            }

            std::string userID = body["userID"];
            std::string password = body["password"];
            json response = authenticateUser(userID, password);

            MyLogger::info("Authentication response: " + response.dump());
            res.set_content(response.dump(), "application/json");

        } catch (const std::exception& e) {
            MyLogger::error("Request error: " + std::string(e.what()));
            res.status = 500;
            res.set_content(R"({"status": "error", "message": "Server error"})", "application/json");
        } });

        MyLogger::info("Authentication Server running on " + server_ip + ":" + std::to_string(server_port) + "...");

        // Check if listen was successful
        if (!svr.listen(server_ip, server_port)) {
            MyLogger::error("Failed to start server on " + server_ip + ":" + std::to_string(server_port));
            return 1;
        }
        
    // Cleanup
    if (redis_ctx)
    {
        redisFree(redis_ctx);
    }

    return 0;
}
