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
#include "../notification_server/notification_server.hpp"
#include "./logger/Mylogger.h"
#include <nlohmann/json.hpp> // JSON parsing
#include <iostream>
#include <mutex>
#include <bits/stdc++.h>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio.hpp>
#include <iostream>

// Make sure to include these namespaces or qualify appropriately.
namespace beast = boost::beast;     // from <boost/beast.hpp>
namespace http  = beast::http;        // from <boost/beast/http.hpp>
namespace asio  = boost::asio;        // from <boost/asio.hpp>
using tcp       = asio::ip::tcp;
using json = nlohmann::json;

// server config file
const std::string server_config_file = "server_config.json";
const std::string public_key_file = "public.pem";  // read this from server config file
const std::string server_ip = "127.0.0.1"; // read this from server config file
const int server_port = 35000; // read this from server config file

// Simulated metadata store
std::vector<std::string> metadata_servers;
std::vector<std::string> block_servers;
std::vector<std::string> notification_servers;
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
            block_servers = config["block_servers"].get<std::vector<std::string>>();
            notification_servers = config["notification_servers"].get<std::vector<std::string>>();
            MyLogger::info("Loaded " + std::to_string(metadata_servers.size()) + " servers from config file");
        }
        catch (const std::exception &e)
        {
            MyLogger::error("Exception in load_server_config: " + std::string(e.what()));
        }
    }

}

namespace Authentication
{

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

namespace Database_handler
{

    std::vector<std::vector<std::string>> server_groups;  // server groups for metadata storage
    inline std::vector<json> blockserver_lists;  // server groups for block storage required for file storage
    const std::string database_server_config_file = "meta_server_config.json"; // metadatastorage server config file
    const std::string block_server_config_file = "block_server_config.json"; // block server config file

    bool load_server_config(const std::string &filename, std::vector<std::vector<std::string>> &server_groups) // metadatastorage server loader
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

    inline void load_blockserver_config(const std::string &filepath = block_server_config_file) // block server config loader
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
            blockserver_lists.push_back(value);
        }

        MyLogger::info("Loaded blockserver config with " + std::to_string(blockserver_lists.size()) + " server groups.");
    }

    std::vector<std::string> &select_metastorage_group(const std::string &key)
    {
        static std::hash<std::string> hasher;
        size_t idx = hasher(key) % server_groups.size();
        return server_groups[idx];
    }

    json &select_block_server_group(const std::string &key)
    {
        static std::hash<std::string> hasher;
        size_t idx = hasher(key) % blockserver_lists.size();
        return blockserver_lists[idx];
    }

    distributed_KV::Response get_directory_metadata(const std::string &key)
    {
        auto &servers = select_metastorage_group(key);
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
        return distributed_KV::set(select_metastorage_group(key), key, value);
    }

    distributed_KV::Response delete_directory_metadata(const std::string &key)
    {
        return distributed_KV::del(select_metastorage_group(key), key);
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
            try
            {
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
            {"parent_dir", parent_dir}};

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

    try
    {

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

        try
        {
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
    catch (const std::exception &e)
    {
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
            {"version", 1},
            {"servers", chosen_servers}};

        // Add to parent's files
        parent_metadata["files"][filename] = chosen_servers;

        // Store both file and updated parent directory metadata
        auto file_ok = Database_handler::set_directory_metadata(key, file_meta.dump());
        auto parent_ok = Database_handler::set_directory_metadata(parent_key, parent_metadata.dump());

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

void update_file(const httplib::Request &req, httplib::Response &res)
{
    MyLogger::info("Received file update request");

    std::string userID;
    if (!Authentication::authenticate_request(req, res, userID))
        return;

    try
    {
        json body_json = json::parse(req.body);

        if (!body_json.contains("path") || !body_json.contains("version"))
        {
            res.status = 400;
            res.set_content(R"({"error": "Missing path or version"})", "application/json");
            MyLogger::warning("File update failed: Missing path or version");
            return;
        }

        std::string file_path = body_json["path"];
        int client_version = body_json["version"];
        std::string key = userID + ":" + file_path;

        auto kv_response = Database_handler::get_directory_metadata(key);
        if (!kv_response.success)
        {
            res.status = 404;
            res.set_content(R"({"error": "File not found"})", "application/json");
            MyLogger::warning("File update failed: File not found: " + key);
            return;
        }

        json metadata;
        try
        {
            metadata = json::parse(kv_response.value);
        }
        catch (const std::exception &e)
        {
            res.status = 500;
            MyLogger::error("Failed to parse file metadata: " + std::string(e.what()));
            res.set_content(R"({"error": "Failed to parse file metadata"})", "application/json");
            return;
        }

        int current_version = metadata.value("version", 1); // default to 1 for safety

        json &block_servers = Database_handler::select_block_server_group(key);

        if (client_version == current_version)
        {
            res.status = 200;
            json response_json = {
                {"status", "ok"},
                {"servers", block_servers}};
            res.set_content(response_json.dump(), "application/json");
            MyLogger::info("File update accepted: " + key);
        }
        else
        {
            res.status = 409;
            json response_json = {
                {"status", "outdated"},
                {"current_version", current_version},
                {"servers", block_servers}};
            res.set_content(response_json.dump(), "application/json");
            MyLogger::warning("File update rejected due to version mismatch: " + key);
        }
    }
    catch (const std::exception &e)
    {
        res.status = 500;
        MyLogger::error("Exception in update_file: " + std::string(e.what()));
        res.set_content(R"({"error": "Internal server error"})", "application/json");
    }
}

// Function to initialize root directories
void initialize_root_directories()
{
    std::lock_guard<std::mutex> lock(metadata_lock);
    std::string userID = "user1";
    std::string root_key = userID + ":dropbox";

    if (!directory_metadata.count(root_key))
    {
        directory_metadata[root_key] = {
            {"owner", userID},
            {"timestamp", std::chrono::system_clock::to_time_t(std::chrono::system_clock::now())},
            {"subdirectories", json::object()},
            {"files", json::object()},
            {"endpoints", json::array()}, // No need for storage servers for root
            {"parent_dir", ""}};
        MyLogger::info("Root directory created manually for " + userID);
    }
}

// Example function to send notification via HTTP POST
void send_notification(nlohmann::json &message) {
    // Iterate over all notification server entries
    for (const auto &server : notification_servers) {
        try {
            // Assuming each 'server' has members "ip" and "port".
            std::string server_ip = server.ip; // or server["ip"] if using json
            unsigned short server_port = server.port; // or static_cast<unsigned short>(server["port"])

            // Create an io_context for this connection.
            asio::io_context ioc;

            // Resolve the server address and port.
            tcp::resolver resolver(ioc);
            auto const results = resolver.resolve(server_ip, std::to_string(server_port));

            // Create a TCP stream using Boost.Beast.
            beast::tcp_stream stream(ioc);

            // Establish a connection using one of the endpoints.
            stream.connect(results);

            // Prepare the HTTP POST request to the "/broadcast" endpoint.
            http::request<http::string_body> req{http::verb::post, "/broadcast", 11};
            req.set(http::field::host, server_ip);
            req.set(http::field::user_agent, BOOST_BEAST_VERSION_STRING);
            req.set(http::field::content_type, "application/json");
            req.body() = message.dump();
            req.prepare_payload();

            // Send the HTTP request to the server.
            http::write(stream, req);

            // Buffer for reading the response.
            beast::flat_buffer buffer;
            // Container for the response.
            http::response<http::dynamic_body> res;
            // Receive the HTTP response.
            http::read(stream, buffer, res);
            std::cout << "Response from " << server_ip << ": " << res << std::endl;

            // Gracefully close the socket.
            beast::error_code ec;
            stream.socket().shutdown(tcp::socket::shutdown_both, ec);
            if (ec && ec != beast::errc::not_connected) {
                throw beast::system_error{ec};
            }
        } catch (const std::exception &e) {
            std::cerr << "Error sending notification to server: " << e.what() << "\n";
        }
    }
}


// Function to handle block server confirmation
// it will send the confirmation to the notification server
void block_server_confirmation(const httplib::Request &req, httplib::Response &res)
{   

    MyLogger::info("Received block server confirmation request");
    json message = {
        {"type", "block_server_confirmation"}
    };

    send_notification(message);
}

int main()
{

    httplib::Server svr;

    Initiation::load_server_config(server_config_file);
    initialize_root_directories();
    Database_handler::load_server_config(Database_handler::database_server_config_file, Database_handler::server_groups);
    Database_handler::load_blockserver_config(Database_handler::block_server_config_file);

    // Routes
    svr.Post("/create-directory", create_directory);
    // svr.Get("/list-directory/(.*)", list_directory);
    // svr.Post("/create-file/(.*)", create_file);
    // svr.Put("/confirmation/(.*)", block_server_confirmation);

    // Start server

    MyLogger::info("Server started on http://" + server_ip + ":" + to_string(server_port));
    svr.listen(server_ip, server_port);
}


// api to get servers

// api to delete

// check atomic locking of database that will be used