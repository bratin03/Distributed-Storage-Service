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
#pragma once
#include "../../utils/libraries/cpp-httplib/httplib.h"
#include "../../utils/libraries/jwt-cpp/include/jwt-cpp/jwt.h"
#include "../../utils/Distributed_KV/client_lib/kv.hpp"
#include "./logger/Mylogger.h"
#include <nlohmann/json.hpp> // JSON parsing
#include <iostream>
#include <unordered_map>
#include <mutex>
#include <bits/stdc++.h>

using json = nlohmann::json;


// server config file
const std::string server_config_file = "server_config.json";
const std::string public_key_file = "public.pem";

// Simulated metadata store
std::unordered_map<std::string, json> directory_metadata;
std::unordered_map<std::string, json> file_metadata;
std::vector<std::string> metadata_servers;
std::mutex server_lock;
std::mutex metadata_lock;

// Public key for JWT verification
// Load RSA Public Key
namespace Initiation
{

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
            MyLogger::info("Loaded public key from file: " + filename);
            return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
        }
        catch (const std::exception &e)
        {
            MyLogger::error("Exception in loadKey: " + std::string(e.what()));
            throw;
        }
    }

    // Load Public Key


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
            
            MyLogger::info("Loaded " + std::to_string(metadata_servers.size())+ " servers from config file" );
        }
        catch (const std::exception &e)
        {
            MyLogger::error("Exception in load_server_config: " + std::string(e.what()));
        }
    }

}



namespace Authentication{
    
    const std::string PUBLIC_KEY = Initiation::loadKey(public_key_file);
    
    std::optional<std::string> verify_jwt(const std::string &token)
    {
        // Function to verify JWT token and extract userID
        // Ensure the token has three parts
        MyLogger::debug("Inside verify_jwt");
        if (token.empty())
        {
            MyLogger::error("JWT Format Error: Empty token");
            return std::nullopt;
        }
        
        if (std::count(token.begin(), token.end(), '.') != 2)
        {
            MyLogger::error("JWT Format Error: Incorrect token structure");
            return std::nullopt;
        }
        
        MyLogger::debug("JWT Format Correct");
        
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
    bool authenticate_request(const httplib::Request &req, httplib::Response &res, std::string &userID)
    {
        
        MyLogger::debug("Inside authenticate_request");
        
        if (!req.has_header("Authorization"))
        {
            res.status = 401;
            res.set_content(R"({"error": "Missing authentication token"})", "application/json");
            MyLogger::error("Authentication failed: Missing token");
            return false;
        }
        MyLogger::debug("Authorization header found");
        
        std::string token = req.get_header_value("Authorization");
        
        if (token.rfind("Bearer ", 0) == 0)
        {
            MyLogger::debug("Bearer found");
            token = token.substr(7); // Remove "Bearer " prefix
        }
        MyLogger::debug("Token trimed: " + token);
        
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
    
}



namespace Database_handler{

    std::vector<std::vector<std::string>> server_groups;
    const std::string database_server_config_file = "database_server_config.json";

    bool load_server_config(const std::string &filename, std::vector<std::vector<std::string>> &server_groups)
    {
        std::ifstream file(filename);
        if (!file.is_open())
        {
            MyLogger::error("Failed to open server config file: " + filename);
            return false;
        }

        try
        {
            nlohmann::json config_json;
            file >> config_json;

            for (const auto &entry : config_json.items())
            {
                const auto &server_list = entry.value();
                if (!server_list.is_array())
                {
                    MyLogger::warning("Skipping invalid entry in server config: " + entry.key());
                    continue;
                }

                std::vector<std::string> group;
                for (const auto &endpoint : server_list)
                {
                    if (endpoint.is_string())
                    {
                        group.push_back(endpoint.get<std::string>());
                    }
                }
                server_groups.push_back(std::move(group));
            }

            MyLogger::info("Loaded " + std::to_string(server_groups.size()) + " server groups from config.");
            return true;
        }
        catch (const std::exception &e)
        {
            MyLogger::error("Failed to parse server config JSON: " + std::string(e.what()));
            return false;
        }
    }


    std::vector<std::string> &select_server_group(const std::string &key)
    {
        static std::hash<std::string> hasher;
        size_t idx = hasher(key) % server_groups.size();
        return server_groups[idx];
    }


    distributed_KV::Response get_directory_metadata(const std::string &key)
    {
        auto &servers = select_server_group(key);
        distributed_KV::Response res = distributed_KV::get(servers, key);

        if (!res.success)
        {
            MyLogger::warning("KV GET failed for key: " + key + " | Error: " + res.err);
            return res;
        }

        return res;
    }

    distributed_KV::Response set_directory_metadata(const std::string &key, const json &metadata)
    {
        std::string value = metadata.dump();
        return distributed_KV::set(select_server_group(key), key, value);
    }

    distributed_KV::Response delete_directory_metadata(const std::string &key)
    {
        return distributed_KV::del(select_server_group(key), key);
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

void create_directory(const httplib::Request &req, httplib::Response &res)
{
    MyLogger::info("Received directory creation request");
    std::string userID;
    if (!Authentication::authenticate_request(req, res, userID))
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

        json existing_metadata;
        auto get_result = Database_handler::get_directory_metadata(key);
        if (get_result.success)
        {
            res.status = 400;
            MyLogger::warning("Directory already exists: " + key);
            res.set_content(R"({"error": "Directory already exists"})", "application/json");
            return;
        }

        std::string parent_dir, parent_key;
        json parent_metadata;
        if (size_t last_slash = dir_id.find_last_of('/'); last_slash != std::string::npos)
        {
            parent_dir = dir_id.substr(0, last_slash);
            parent_key = userID + ":" + parent_dir;

            auto parent_get_result = Database_handler::get_directory_metadata(parent_key);
            if (!parent_get_result.success)
            {
                res.status = 404;
                MyLogger::warning("Parent directory not found: " + parent_key);
                res.set_content(R"({"error": "Parent directory not found"})", "application/json");
                return;
            }
            try{
                parent_metadata = json::parse(parent_get_result.value);
            }
            catch (const std::exception &e)
            {
                res.status = 500;
                MyLogger::error("Failed to parse parent directory metadata: " + std::string(e.what()));
                res.set_content(R"({"error": "Failed to parse parent directory metadata"})", "application/json");
                return;
            }

        }

        // Choose 3 block servers for this directory
        std::vector<std::string> chosen_servers;
        select_round_robin_servers(chosen_servers);

        json new_metadata = {
            {"owner", userID},
            {"timestamp", std::chrono::system_clock::to_time_t(std::chrono::system_clock::now())},
            {"subdirectories", json::object()},
            {"files", json::object()},
            {"endpoints", chosen_servers},
            {"parent_dir", parent_dir}
        };

        auto set_result = Database_handler::set_directory_metadata(key, new_metadata);
        if (!set_result.success)
        {
            res.status = 500;
            MyLogger::error("Failed to write new directory metadata: " + set_result.err);
            res.set_content(R"({"error": "Failed to create directory"})", "application/json");
            return;
        }

        if (!parent_key.empty())
        {
            // Update parent metadata
            parent_metadata["subdirectories"][dir_id] = chosen_servers;
            auto update_parent_result = Database_handler::set_directory_metadata(parent_key, parent_metadata);
            if (!update_parent_result.success)
            {
                MyLogger::warning("Directory created but failed to update parent metadata: " + update_parent_result.err);
                // Optional: roll back new dir?
            }
        }

        res.status = 200;
        res.set_content(
            R"({"message": "Directory created", "metadata": )" + new_metadata.dump() + "}",
            "application/json");

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
void list_directory(const httplib::Request &req, httplib::Response &res)
{
    MyLogger::info("Received directory listing request");
    std::string userID;
    if (!Authentication::authenticate_request(req, res, userID))
        return;

    try{

        std::string dir_id = req.matches[1];
        std::string key = userID + ":" + dir_id;
        
        json metadata;
        auto kv_response = Database_handler::get_directory_metadata(key);
        if (!kv_response.success)
        {
            res.status = 404;
            res.set_content(R"({"error": "Directory not found"})", "application/json");
            return;
        }
        
        try{
            metadata = json::parse(kv_response.value);
        }
        catch (const std::exception &e)
        {
            res.status = 500;
            MyLogger::error("Failed to parse directory metadata: " + std::string(e.what()));
            res.set_content(R"({"error": "Failed to parse metadata"})", "application/json");
            return;
        }
        
        
            res.set_content(metadata.dump(), "application/json");
            MyLogger::info("Listed directory from KV store: " + key);
        
    }
    catch (const std::exception &e){
            res.status = 500;
            MyLogger::error("Exception in list directory: " + std::string(e.what()));
            res.set_content(R"({"error": "Internal server error"})", "application/json");
    }
}





// Function to create a file
void create_file(const httplib::Request &req, httplib::Response &res)
{
    MyLogger::info("Received file creation request");

    std::string userID;
    if (!Authentication::authenticate_request(req, res, userID))
        return;

    try
    {
        json body_json = json::parse(req.body);
        if (!body_json.contains("path"))
        {
            res.status = 400;
            MyLogger::warning("File creation failed: Missing file path");
            res.set_content(R"({"error": "Missing path"})", "application/json");
            return;
        }

        std::string file_path = body_json["path"];
        std::string key = userID + ":" + file_path;

        size_t last_slash = file_path.find_last_of('/');
        if (last_slash == std::string::npos)
        {
            res.status = 400;
            MyLogger::warning("File creation failed: Invalid file path (no parent directory)");
            res.set_content(R"({"error": "Invalid file path"})", "application/json");
            return;
        }

        std::string parent_dir = file_path.substr(0, last_slash);
        std::string parent_key = userID + ":" + parent_dir;
        std::string filename = file_path.substr(last_slash + 1);

        // Fetch parent directory metadata
        auto parent_res = Database_handler::get_directory_metadata(parent_key);
        if (!parent_res.success)
        {
            res.status = 404;
            MyLogger::warning("File creation failed: Parent directory not found");
            res.set_content(R"({"error": "Parent directory not found"})", "application/json");
            return;
        }

        json parent_metadata;
        try
        {
            parent_metadata = json::parse(parent_res.value);
        }
        catch (const std::exception &e)
        {
            res.status = 500;
            MyLogger::error("Failed to parse parent directory metadata: " + std::string(e.what()));
            res.set_content(R"({"error": "Failed to parse parent directory metadata"})", "application/json");
            return;
        }

        // Check for duplicate file
        if (parent_metadata["files"].contains(filename))
        {
            res.status = 409;
            MyLogger::warning("File already exists: " + filename);
            res.set_content(R"({"error": "File already exists"})", "application/json");
            return;
        }

        // Choose servers to assign the file to
        std::vector<std::string> chosen_servers(metadata_servers.begin(), metadata_servers.begin() + 3);

        json file_meta = {
            {"parent_dir", parent_dir},
            {"filename", filename},
            {"owner", userID},
            {"timestamp", std::chrono::system_clock::to_time_t(std::chrono::system_clock::now())},
            {"size", 0},
            {"servers", chosen_servers}
        };

        // Add to parent's files
        parent_metadata["files"][filename] = chosen_servers;

        // Store both file and updated parent directory metadata
        auto file_ok = Database_handler::set_directory_metadata(key, file_meta.dump());
        auto parent_ok = Database_handler::set_directory_metadata (parent_key, parent_metadata.dump());

        if (!file_ok.success || !parent_ok.success)
        {
            res.status = 500;
            MyLogger::error("Failed to update KV store for file or parent directory");
            res.set_content(R"({"error": "Failed to store file metadata"})", "application/json");
            return;
        }

        res.set_content(R"({"message": "File created", "metadata": )" + file_meta.dump() + "}", "application/json");
        MyLogger::info("File created: " + key);
    }
    catch (const std::exception &e)
    {
        MyLogger::error("Exception in create_file: " + std::string(e.what()));
        res.status = 500;
        res.set_content(R"({"error": "Internal server error"})", "application/json");
    }
}






// Function to initialize root directories
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

    httplib::Server svr;

    Initiation::load_server_config(server_config_file);
    initialize_root_directories();
    Database_handler::load_server_config(Database_handler::database_server_config_file, Database_handler::server_groups);

    // Routes
    svr.Post("/create-directory", create_directory);
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