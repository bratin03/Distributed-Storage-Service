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

// Internal modules
#include "logger/Mylogger.h"
#include "./initiation/initiation.hpp"
#include "./authentication/authentication.hpp"
#include "../notification_server/notification_server.hpp"

// Third-party libraries
#include "../../utils/libraries/cpp-httplib/httplib.h"
#include "../../utils/libraries/jwt-cpp/include/jwt-cpp/jwt.h"
#include "../../utils/Distributed_KV/client_lib/kv.hpp"

// JSON and standard
#include <nlohmann/json.hpp>
#include <iostream>
#include <mutex>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio.hpp>
#include <bits/stdc++.h>

// Make sure to include these namespaces or qualify appropriately.
namespace beast = boost::beast; // from <boost/beast.hpp>
namespace http = beast::http;   // from <boost/beast/http.hpp>
namespace asio = boost::asio;   // from <boost/asio.hpp>
using tcp = asio::ip::tcp;
using json = nlohmann::json;

namespace Database_handler
{

    std::vector<std::string> &select_metastorage_group(const std::string &key)
    {
        static std::hash<std::string> hasher;
        size_t idx = hasher(key) % Initiation::metastorage_groups.size();
        return Initiation::metastorage_groups[idx];
    }

    json &select_block_server_group(const std::string &key)
    {
        static std::hash<std::string> hasher;
        size_t idx = hasher(key) % Initiation::blockserver_lists.size();
        return Initiation::blockserver_lists[idx];
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
namespace utility_functions
{
    // Round-Robin Selection Algorithm
    inline void select_round_robin_servers(std::vector<std::string> &selected, int count = 3)
    {
        static size_t index = 0;
        // std::lock_guard<std::mutex> lock(server_lock);

        for (int i = 0; i < count; i++)
        {
            selected.push_back(Initiation::metadata_servers[(index + i) % Initiation::metadata_servers.size()]);
        }
        index = (index + count) % Initiation::metadata_servers.size();
    }

    inline bool is_tombstoned(const json &metadata) {
        return metadata.contains("deleted") && metadata["deleted"].get<bool>() == true;
    }

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
        if ( get_result.success )
        {
            MyLogger::warning("Directory already exists: " + key);
            
            try
            {
                existing_metadata = json::parse(get_result.value);
            }
            catch (const std::exception &e)
            {
                res.status = 500;
                MyLogger::error("Failed to parse existing directory metadata: " + std::string(e.what()));
                res.set_content(R"({"error": "Failed to parse existing directory metadata"})", "application/json");
                return;
            }

            if(utility_functions::is_tombstoned(existing_metadata))
            {
                // If the directory is tombstoned, we can proceed to create it again
                MyLogger::info("Directory is tombstoned, proceeding to create it again: " + key);
            }
            else
            {
                
                res.status = 400;
                res.set_content(R"({"error": "Directory already exists"})", "application/json");
                return;
            }
        }

        std::string parent_dir, parent_key;
        json parent_metadata;
        
        size_t last_slash = dir_id.find_last_of('/');
        if (last_slash == std::string::npos)
        {
            res.status = 400;
            MyLogger::warning("File creation failed: Invalid file path (no parent directory)");
            res.set_content(R"({"error": "Invalid file path"})", "application/json");
            return;
        }


        
        parent_dir = dir_id.substr(0, last_slash);
        parent_key = userID + ":" + parent_dir;

        auto parent_get_result = Database_handler::get_directory_metadata(parent_key);
        if (!parent_get_result.success)  // Check if parent directory exists
        {
            res.status = 404;
            MyLogger::warning("Parent directory not found: " + parent_key + " " + parent_get_result.err);
            res.set_content(R"({"error": "Parent directory not found"})", "application/json");
            return;
        }
        
        // Parse parent metadata
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
        
        // Check if parent directory is tombstoned
        if(utility_functions::is_tombstoned(parent_metadata))  
        {
            res.status = 400;
            MyLogger::warning("Parent directory is tombstoned: " + parent_key);
            res.set_content(R"({"error": "Parent directory is tombstoned"})", "application/json");
            return;
        }
    

        // Choose 3 block servers for this directory
        std::vector<std::string> chosen_servers;
        utility_functions::select_round_robin_servers(chosen_servers);

        json new_metadata = {
            {"owner", userID},
            {"timestamp", std::chrono::system_clock::to_time_t(std::chrono::system_clock::now())},
            {"subdirectories", json::object()},
            {"files", json::object()},
            {"endpoints", chosen_servers},
            {"parent_dir", parent_dir},
            {"deleted", false} // New field to indicate if the directory is deleted
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

    // try
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

        json existing_metadata;
        auto get_result = Database_handler::get_directory_metadata(key);
        if (get_result.success)
        {
            MyLogger::warning("File already exists: " + key);
            try
            {
                existing_metadata = json::parse(get_result.value);
            }
            catch (const std::exception &e)
            {
                res.status = 500;
                MyLogger::error("Failed to parse existing file metadata: " + std::string(e.what()));
                res.set_content(R"({"error": "Failed to parse existing file metadata"})", "application/json");
                return;
            }

            if (utility_functions::is_tombstoned(existing_metadata))
            {
                // If the file is tombstoned, we can proceed to create it again
                MyLogger::info("File is tombstoned, proceeding to create it again: " + key);
            }
            else
            {
                res.status = 400;
                res.set_content(R"({"error": "File already exists"})", "application/json");
                return;
            }
        }


        size_t last_slash = file_path.find_last_of('/');
        if (last_slash == std::string::npos)
        {
            res.status = 400;
            MyLogger::warning("File creation failed: Invalid file path (no parent directory)");
            res.set_content(R"({"error": "Invalid file path"})", "application/json");
            return;
        }

        /*
         struct Response
            {
                std::string value; // Contains the response payload if applicable.
                std::string err;   // Contains error message if any.
                bool success;
            };
        */

        std::string parent_dir = file_path.substr(0, last_slash);
        std::string parent_key = userID + ":" + parent_dir;
        json parent_metadata;
        std::string filename = file_path.substr(last_slash + 1);

        // Fetch parent directory metadata
        auto parent_res = Database_handler::get_directory_metadata(parent_key);
        if (!parent_res.success)
        {
            res.status = 404;
            MyLogger::warning("File creation failed: Parent directory not found "+ parent_dir + " " + parent_res.err);
            res.set_content(R"({"error": "Parent directory not found"})", "application/json");
            return;
        }

        try
        {
            parent_metadata = json::parse(parent_res.value);
            if (parent_metadata.is_string()) {
                try {
                    parent_metadata = json::parse(parent_metadata.get<std::string>());
                } catch (const std::exception &e) {
                    MyLogger::error("Failed to parse inner metadata: " + std::string(e.what()));
                    res.status = 500;
                    res.set_content(R"({"error": "Failed to parse inner metadata"})", "application/json");
                    return;
                }
            }
        }
        catch (const std::exception &e)
        {
            res.status = 500;
            MyLogger::error("Failed to parse parent directory metadata: " + std::string(e.what()));
            res.set_content(R"({"error": "Failed to parse parent directory metadata"})", "application/json");
            return;
        }

        // Debug print the metadata
        MyLogger::debug("parent metadata: " + parent_metadata.dump());
        // Check if parent directory is tombstoned
        if (utility_functions::is_tombstoned(parent_metadata))
        {
            res.status = 400;
            MyLogger::warning("Parent directory is tombstoned: " + parent_key);
            res.set_content(R"({"error": "Parent directory is tombstoned"})", "application/json");
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

        MyLogger::info("Creating file: " + key);

        std::vector<std::string> chosen_servers;
        utility_functions::select_round_robin_servers(chosen_servers);
 
        json file_meta = {
            {"parent_dir", parent_dir},
            {"filename", filename},
            {"owner", userID},
            {"timestamp", std::chrono::system_clock::to_time_t(std::chrono::system_clock::now())},
            {"size", 0},
            {"version", 1},
            {"servers", chosen_servers},
            {"deleted", false} // New field to indicate if the file is deleted
            
        };

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
    // catch (const std::exception &e)
    // {
    //     MyLogger::error("Exception in create_file: " + std::string(e.what()));
    //     res.status = 500;
    //     res.set_content(R"({"error": "Internal server error"})", "application/json");
    // }
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

// // // Example function to send notification via HTTP POST
// // void send_notification(nlohmann::json &message) {
// //     // Iterate over all notification server entries
// //     for (const auto &server : notification_servers) {
// //         try {
// //             // Assuming each 'server' has members "ip" and "port".
// //             std::string server_ip = server.ip; // or server["ip"] if using json
// //             unsigned short server_port = server.port; // or static_cast<unsigned short>(server["port"])

// //             // Create an io_context for this connection.
// //             asio::io_context ioc;

// //             // Resolve the server address and port.
// //             tcp::resolver resolver(ioc);
// //             auto const results = resolver.resolve(server_ip, std::to_string(server_port));

// //             // Create a TCP stream using Boost.Beast.
// //             beast::tcp_stream stream(ioc);

// //             // Establish a connection using one of the endpoints.
// //             stream.connect(results);

// //             // Prepare the HTTP POST request to the "/broadcast" endpoint.
// //             http::request<http::string_body> req{http::verb::post, "/broadcast", 11};
// //             req.set(http::field::host, server_ip);
// //             req.set(http::field::user_agent, BOOST_BEAST_VERSION_STRING);
// //             req.set(http::field::content_type, "application/json");
// //             req.body() = message.dump();
// //             req.prepare_payload();

// //             // Send the HTTP request to the server.
// //             http::write(stream, req);

// //             // Buffer for reading the response.
// //             beast::flat_buffer buffer;
// //             // Container for the response.
// //             http::response<http::dynamic_body> res;
// //             // Receive the HTTP response.
// //             http::read(stream, buffer, res);
// //             std::cout << "Response from " << server_ip << ": " << res << std::endl;

// //             // Gracefully close the socket.
// //             beast::error_code ec;
// //             stream.socket().shutdown(tcp::socket::shutdown_both, ec);
// //             if (ec && ec != beast::errc::not_connected) {
// //                 throw beast::system_error{ec};
// //             }
// //         } catch (const std::exception &e) {
// //             std::cerr << "Error sending notification to server: " << e.what() << "\n";
// //         }
// //     }
// // }

// // Function to handle block server confirmation
// // it will send the confirmation to the notification server
// void block_server_confirmation(const httplib::Request &req, httplib::Response &res)
// {

//     MyLogger::info("Received block server confirmation request");
//     json message = {
//         {"type", "block_server_confirmation"}
//     };

//     send_notification(message);
// }

int main()
{

    httplib::Server svr;

    Initiation::initialize("config/server_config.json");
    // Routes
    svr.Post("/create-directory", create_directory);
    svr.Get("/list-directory/(.*)", list_directory);
    svr.Post("/create-file", create_file);
    // svr.Put("/confirmation/(.*)", block_server_confirmation);

    // Start server

    MyLogger::info("Server started");
    svr.listen(Initiation::server_ip, Initiation::server_port);
}

// api to get servers

// api to delete

// check atomic locking of database that will be used