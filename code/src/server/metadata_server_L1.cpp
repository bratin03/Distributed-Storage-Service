/*
g++ -std=c++17 -I../../utils/libraries/jwt-cpp/include metadata_service_L1.cpp -o metadata_service -lssl -lcrypto

*/

/*
    Example metadata structure
    :
        "owner": userID,
        "timestamp": 1690000000, // Unix timestamp
        "subdirectories": {
            "subdir1": ["IP1:Port1", "IP2:Port2"],
            "subdir2": ["IP3:Port3"]
        },
        "files": {
            "file1.txt": ["IP4:Port4"],
            "file2.txt": ["IP5:Port5", "IP6:Port6"]
        },
        "endpoints": ["IP1:Port1", "IP2:Port2", "IP3:Port3"]


    Client will need to parse this metadata to get the list of subdirectories and files end points then hit those end points
*/

#define CPPHTTPLIB_OPENSSL_SUPPORT
#include "../../utils/libraries/cpp-httplib/httplib.h"
#include "../../utils/libraries/jwt-cpp/include/jwt-cpp/jwt.h"
#include "./logger/Mylogger.h"
#include <nlohmann/json.hpp> // JSON parsing
#include <iostream>
#include <unordered_map>
#include <mutex>
#include <bits/stdc++.h>

using json = nlohmann::json;
using namespace httplib;

// server config file
const std::string server_config_file = "server_config.json";

// Simulated metadata store
std::unordered_map<std::string, json> directory_metadata;
std::unordered_map<std::string, json> file_metadata;
std::mutex metadata_lock;

// Public key for JWT verification
// Load RSA Public Key
std::string loadKey(const std::string &filename)
{
    try
    {
        std::ifstream file(filename, std::ios::in);
        if (!file.is_open())
        {
            MyLogger::error("Failed to open key file: " + filename);
            throw std::runtime_error("Failed to open key file");
        }
        return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
    }
    catch (const std::exception &e)
    {
        MyLogger::error("Exception in loadKey: " + std::string(e.what()));
        throw;
    }
}

// Load Public Key
const std::string PUBLIC_KEY = loadKey("public.pem");

std::vector<std::string> metadata_servers;
std::mutex server_lock;

// Function to load servers from JSON config file
void load_server_config(const std::string &filename)
{
    try
    {
        std::ifstream file(filename);
        if (!file.is_open())
        {
            MyLogger::error("Failed to open server config file: " + filename);
            throw std::runtime_error("Failed to open server config file");
        }

        nlohmann::json config;
        file >> config;

        std::lock_guard<std::mutex> lock(server_lock);
        metadata_servers = config["servers"].get<std::vector<std::string>>();
    }
    catch (const std::exception &e)
    {
        MyLogger::error("Exception in load_server_config: " + std::string(e.what()));
    }
}


// Round-Robin Selection Algorithm
void select_round_robin_servers(std::vector<std::string> &selected, int count = 3)
{
    static size_t index = 0;
    std::lock_guard<std::mutex> lock(server_lock);

    for (int i = 0; i < count; i++)
    {
        selected.push_back(metadata_servers[(index + i) % metadata_servers.size()]);
    }
    index = (index + count) % metadata_servers.size();
}

std::optional<std::string> verify_jwt(const std::string &token)
{
    // Function to verify JWT token and extract userID
    // Ensure the token has three parts
    if (std::count(token.begin(), token.end(), '.') != 2)
    {
        MyLogger::error("JWT Format Error: Incorrect token structure");
        return std::nullopt;
    }

    try
    {
        auto decoded = jwt::decode(token);
        auto verifier = jwt::verify()
                            .allow_algorithm(jwt::algorithm::rs256(PUBLIC_KEY))
                            .with_issuer("auth-server");

        verifier.verify(decoded);
        return decoded.get_payload_claim("userID").as_string();
    }
    catch (const std::system_error &e)
    { // Catching system_error from jwt-cpp
        MyLogger::error("JWT Verification Failed: " + std::string(e.what()));
        return std::nullopt;
    }
    catch (const std::exception &e)
    { // Catching general exceptions
        MyLogger::error("JWT Verification Failed: " + std::string(e.what()));
        return std::nullopt;
    }
}

// Middleware to handle authentication
bool authenticate_request(const Request &req, Response &res, std::string &userID)
{
    if (!req.has_header("Authorization"))
    {
        res.status = 401;
        res.set_content(R"({"error": "Missing authentication token"})", "application/json");
        MyLogger::error("Authentication failed: Missing token");
        return false;
    }

    std::string token = req.get_header_value("Authorization");

    if (token.rfind("Bearer ", 0) == 0)
    {
        token = token.substr(7); // Remove "Bearer " prefix
    }

    auto verified_user = verify_jwt(token);
    if (!verified_user)
    {
        res.status = 403;
        res.set_content(R"({"error": "Invalid token"})", "application/json");
        MyLogger::error("Authentication failed: Invalid token");
        return false;
    }

    userID = *verified_user;
    MyLogger::info("Authenticated user: " + userID);
    return true;
}

// Function to handle directory creation
void create_directory(const Request &req, Response &res)
{
    std::string userID;
    if (!authenticate_request(req, res, userID))
        return;
    
    
    try
    {

        json body_json = json::parse(req.body);
        if (!body_json.contains("path"))
        {
            res.status = 400;
            MyLogger::warning("Directory creation failed: Missing path");
            res.set_content(R"({"error": "Missing path"})", "application/json");
            return;
        }
        
        std::string dir_id = body_json["path"];
        std::string key = userID + ":" + dir_id;
        
        std::lock_guard<std::mutex> lock(metadata_lock);
        
        if (directory_metadata.count(key))
        {
            res.status = 400;
            MyLogger::warning("Directory already exists: " + key);
            res.set_content(R"({"error": "Directory already exists"})", "application/json");
            return;
        }
        
        size_t last_slash = dir_id.find_last_of('/');
        std::string parent_dir, parent_key;
        if (last_slash != std::string::npos)
        {
            parent_dir = dir_id.substr(0, last_slash);
            parent_key = userID + ":" + parent_dir;
            
            if (!directory_metadata.count(parent_key))
            {
                res.status = 404;
                MyLogger::warning("Parent directory not found: " + parent_dir);
                res.set_content(R"({"error": "Parent directory not found"})", "application/json");
                return;
            }
        }
        
        std::vector<std::string> chosen_servers;
        select_round_robin_servers(chosen_servers); // choose 3 servers for the directory
        
        directory_metadata[key] = {
            {"owner", userID},
            {"timestamp", std::chrono::system_clock::to_time_t(std::chrono::system_clock::now())},
            {"subdirectories", json::object()},
            {"files", json::object()},
            {"endpoints", chosen_servers},
            {"parent_dir", parent_dir}};
            
            if (!parent_key.empty())
            {
                directory_metadata[parent_key]["subdirectories"][dir_id] = chosen_servers;
            }
            
            res.set_content(R"({"message": "Directory created", "metadata": )" + directory_metadata[key].dump() + "}", "application/json");
            MyLogger::info("Directory created: " + key);
    }
    catch (const std::exception &e)
    {
        MyLogger::error("Exception in create_directory: " + std::string(e.what()));
        res.status = 500;
        res.set_content(R"({"error": "Internal server error"})", "application/json");
    }

}
        
// Function to list a directory
void list_directory(const Request &req, Response &res)
{
    std::string userID;
    if (!authenticate_request(req, res, userID))
        return;

    std::string dir_id = req.matches[1];
    std::string key = userID + ":" + dir_id;

    std::lock_guard<std::mutex> lock(metadata_lock);
    if (!directory_metadata.count(key))
    {
        res.status = 404;
        res.set_content(R"({"error": "Directory not found"})", "application/json");
        MyLogger::warning("Directory not found: " + key);
        return;
    }

    res.set_content(directory_metadata[key].dump(), "application/json");
    MyLogger::info("Listed directory: " + key);
}

// // Function to create a file
// void create_file(const Request &req, Response &res)
// {
//     std::string userID;
//     if (!authenticate_request(req, res, userID))
//         return;

//     // std::string file_path = req.matches[1]; // Extract file path from request
//     json body_json = json::parse(req.body);

//     // if (!body_json.contains("file_type"))
//     // {
//     //     res.status = 400;
//     //     res.set_content(R"({"error": "Missing file_type"})", "application/json");
//     //     MyLogger::warning("File creation failed: Missing file type");
//     //     return;
//     // }

//     if (!body_json.contains("path"))
//     {
//         res.status = 400;
//         MyLogger::warning("File creation failed: Missing file path");
//         res.set_content(R"({"error": "Missing path"})", "application/json");
//         return;
//     }

//     std::string file_path = body_json["path"];
//     std::string file_type = body_json["file_type"];
//     std::string key = userID + ":" + file_path;

//     std::lock_guard<std::mutex> lock(metadata_lock);

//     size_t last_slash = file_path.find_last_of('/');
//     if (last_slash == std::string::npos)
//     {
//         res.status = 400;
//         res.set_content(R"({"error": "Invalid file path"})", "application/json");
//         MyLogger::warning("File creation failed: Invalid file path (parent directory not found)");
//         return;
//     }

//     std::string parent_dir = file_path.substr(0, last_slash);
//     std::string parent_key = userID + ":" + parent_dir;

//     if (!directory_metadata.count(parent_key))
//     {
//         res.status = 404;
//         res.set_content(R"({"error": "Parent directory not found"})", "application/json");
//         MyLogger::warning("File creation failed: Parent directory not found");
//         return;
//     }

//     json &parent_metadata = directory_metadata[parent_key];
//     std::string filename = file_path.substr(last_slash + 1);
//     if (parent_metadata["files"].contains(filename))
//     {
//         res.status = 409;
//         res.set_content(R"({"error": "File already exists"})", "application/json");
//         MyLogger::warning("File already exists: " + filename);
//         return;
//     }

//     std::vector<std::string> chosen_servers(metadata_servers.begin(), metadata_servers.begin() + 3);
//     file_metadata[key] = {
//         {"file_type", file_type},
//         {"owner", userID},
//         {"timestamp", std::chrono::system_clock::to_time_t(std::chrono::system_clock::now())},
//         {"size", 0},
//         {"servers", chosen_servers}};

//     parent_metadata["files"][filename] = chosen_servers;

//     res.set_content(R"({"message": "File created", "metadata": )" + file_metadata[key].dump() + "}", "application/json");
//     MyLogger::info("File created: " + file_path);
// }

void initialize_root_directories() {
    std::lock_guard<std::mutex> lock(metadata_lock);  
    std::string userID = "user1";
    std::string root_key = userID + ":dropbox";

    if (!directory_metadata.count(root_key)) {
        directory_metadata[root_key] = {
            {"owner", userID},
            {"timestamp", std::chrono::system_clock::to_time_t(std::chrono::system_clock::now())},
            {"subdirectories", json::object()},
            {"files", json::object()},
            {"endpoints", json::array()},  // No need for storage servers for root
            {"parent_dir", ""}
        };
        MyLogger::info("Root directory created manually for " + userID);
    }
}



int main()
{

    Server svr;

    load_server_config(server_config_file);
    initialize_root_directories();

    // Routes
    svr.Post("/create-directory/(.*)", create_directory);
    // svr.Get("/list-directory/(.*)", list_directory);
    // svr.Post("/create-file/(.*)", create_file);

    // Start server
    MyLogger::info("Server started on http://localhost:8080");
    svr.listen("localhost", 8080);


}

// void heartbeat_handler(const Request &, Response &res) {
//     res.set_content(R"({"status": "alive"})", "application/json");
// }

// api to get servers

// api to update the file

// confirm api

// api to delete

// first delete the files of the subdirectory then


// check atomic locking of database that will be used