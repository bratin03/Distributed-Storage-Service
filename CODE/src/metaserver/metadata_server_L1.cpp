/*
g++ -std=c++17 -I../../utils/libraries/jwt-cpp/include metadata_service_L1.cpp -o metadata_service -lssl -lcrypto


work ->
    notification server integration
    block server versioning


*/

#define CPPHTTPLIB_OPENSSL_SUPPORT

// Internal modules
#include "./logger/Mylogger.h"
#include "./initiation/initiation.hpp"
#include "./authentication/authentication.hpp"
#include "./database_handler/database_handler.hpp"
#include "./deletion_manager/deletion_manager.hpp"
#include "../notification_server/notification_server.hpp"

// Third-party libraries
#include "../../utils/libraries/cpp-httplib/httplib.h"
#include "../../utils/libraries/jwt-cpp/include/jwt-cpp/jwt.h"

// JSON and standard
#include <nlohmann/json.hpp>
#include <iostream>
#include <mutex>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio.hpp>
#include <csignal>
#include <atomic>
#include <chrono>
#include <functional>
#include <string>
#include <sstream>
#include <cstdlib>

namespace beast = boost::beast; // from <boost/beast.hpp>
namespace http = beast::http;   // from <boost/beast/http.hpp>
namespace asio = boost::asio;   // from <boost/asio.hpp>
using tcp = asio::ip::tcp;
using json = nlohmann::json;

void create_directory(const httplib::Request &req, httplib::Response &res)
{
    MyLogger::info("Received directory creation request");
    std::string userID;
    if (!Authentication::authenticate_request(req, res, userID))
    {
        MyLogger::warning("Authentication failed during directory creation");
        // Ensure that the response is set by authentication itself or set a fallback:
        if (res.body.empty())
        {
            res.status = 401;
            res.set_content(R"({"error": "Authentication failed"})", "application/json");
        }
        return;
    }

    try
    {
        json body_json = json::parse(req.body);
        if (!body_json.contains("path"))
        {
            res.status = 400;
            MyLogger::warning("Directory creation failed: Missing path in request body");
            res.set_content(R"({"error": "Missing path"})", "application/json");
            return;
        }

        if (!body_json.contains("device_id"))
        {
            res.status = 400;
            MyLogger::warning("Directory creation failed: Missing device_id in request body");
            res.set_content(R"({"error": "Missing device_id"})", "application/json");
            return;
        }

        std::string device_id = body_json["device_id"];

        std::string dir_id = body_json["path"];
        std::string key = userID + ":" + dir_id;
        MyLogger::debug("Processing directory creation for key: " + key);

        json existing_metadata;
        auto get_result = Database_handler::get_directory_metadata(key);
        if (get_result.success)
        {

            // If the directory is tombstoned, we can proceed to create it again
            // not doing this
            MyLogger::warning("Directory already exists: " + key);
            res.status = 400;
            res.set_content(R"({"error": "Directory already exists"})", "application/json");
            return;
        }

        std::string parent_dir, parent_key;
        json parent_metadata;

        size_t last_slash = dir_id.find_last_of('/');
        if (last_slash == std::string::npos)
        {
            res.status = 400;
            MyLogger::warning("Directory creation failed: Invalid path (no parent directory) for key: " + key);
            res.set_content(R"({"error": "Invalid file path"})", "application/json");
            return;
        }

        parent_dir = dir_id.substr(0, last_slash);
        parent_key = userID + ":" + parent_dir;
        MyLogger::debug("Parent directory key: " + parent_key);

        auto parent_get_result = Database_handler::get_directory_metadata(parent_key);
        if (!parent_get_result.success)
        {
            res.status = 404;
            MyLogger::warning("Parent directory not found for key: " + parent_key + " | Error: " + parent_get_result.err);
            res.set_content(R"({"error": "Parent directory not found"})", "application/json");
            return;
        }

        try
        {
            parent_metadata = json::parse(parent_get_result.value);
            if (parent_metadata.is_string())
            {
                try
                {
                    parent_metadata = json::parse(parent_metadata.get<std::string>());
                }
                catch (const std::exception &e)
                {
                    MyLogger::error("Failed to parse inner metadata for parent directory key: " + parent_key + " | Exception: " + std::string(e.what()));
                    res.status = 500;
                    res.set_content(R"({"error": "Failed to parse inner metadata"})", "application/json");
                    return;
                }
            }
        }
        catch (const std::exception &e)
        {
            res.status = 500;
            MyLogger::error("Failed to parse parent directory metadata for key: " + parent_key + " | Exception: " + std::string(e.what()));
            res.set_content(R"({"error": "Failed to parse parent directory metadata"})", "application/json");
            return;
        }

        // New directory metadata including owner, timestamp, empty subdirectories, files.
        json new_metadata = {
            {"owner", userID},
            {"timestamp", std::chrono::system_clock::to_time_t(std::chrono::system_clock::now())},
            {"subdirectories", json::array()},
            {"files", json::array()}};

        auto set_result = Database_handler::set_directory_metadata(key, new_metadata);
        if (!set_result.success)
        {
            res.status = 500;
            MyLogger::error("Failed to write new directory metadata for key: " + key + " | Error: " + set_result.err);
            res.set_content(R"({"error": "Failed to create directory"})", "application/json");
            return;
        }

        // Update parent directory metadata to include the new directory.
        if (!parent_key.empty()) // check required?????
        {
            // Update parent metadata
            auto &subdirs = parent_metadata["subdirectories"];
            if (!subdirs.is_array())
            {
                res.status = 500;
                MyLogger::error("Parent metadata subdirectories is not an array for key: " + parent_key);
                res.set_content(R"({"error": "Internal server error"})", "application/json");
                return;
            }
            subdirs.push_back(dir_id);
            MyLogger::debug("Updated parent metadata with new subdirectory: " + dir_id);

            auto update_parent_result = Database_handler::set_directory_metadata(parent_key, parent_metadata);
            if (!update_parent_result.success)
            {
                MyLogger::warning("Directory created but failed to update parent metadata for key: " + parent_key + " | Error: " + update_parent_result.err);
                // Optional: Additional rollback or error handling may be implemented here.
            }
        }

        res.status = 200;
        res.set_content(
            R"({"message": "Directory created", "metadata": )" + new_metadata.dump() + "}",
            "application/json");

        MyLogger::info("Successfully created directory for key: " + key);

        // Notify the notification server about the newly created directory.
        json notification_payload = {
            {"type", "DIR+"},
            {"user_id", userID},
            {"path", dir_id},
            {"device_id", device_id}};
        MyLogger::debug("Broadcasting notification for new directory: " + dir_id);
        Initiation::broadcaster->broadcast(notification_payload);
    }
    catch (const std::exception &e)
    {
        MyLogger::error("Exception in create_directory: " + std::string(e.what()));
        res.status = 500;
        res.set_content(R"({"error": "Internal server error"})", "application/json");
    }
}

void list_directory(const httplib::Request &req, httplib::Response &res)
{
    MyLogger::info("Received directory listing request");
    std::string userID;
    if (!Authentication::authenticate_request(req, res, userID))
    {
        MyLogger::warning("Authentication failed during directory listing");
        if (res.body.empty())
        {
            res.status = 401;
            res.set_content(R"({"error": "Authentication failed"})", "application/json");
        }
        return;
    }

    try
    {
        json body_json = json::parse(req.body);
        if (!body_json.contains("path"))
        {
            res.status = 400;
            MyLogger::warning("Directory listing failed: Missing path in request body");
            res.set_content(R"({"error": "Missing path or version"})", "application/json");
            return;
        }

        std::string dir_id = body_json["path"];
        std::string key = userID + ":" + dir_id;
        MyLogger::debug("Listing directory for key: " + key);

        json metadata;
        auto kv_response = Database_handler::get_directory_metadata(key);
        if (!kv_response.success)
        {
            res.status = 404;
            MyLogger::warning("Directory not found for key: " + key);
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
            MyLogger::error("Failed to parse directory metadata for key: " + key + " | Exception: " + std::string(e.what()));
            res.set_content(R"({"error": "Failed to parse metadata"})", "application/json");
            return;
        }

        res.status = 200;
        res.set_content(metadata.dump(), "application/json");
        MyLogger::info("Successfully listed directory for key: " + key);
    }
    catch (const std::exception &e)
    {
        res.status = 500;
        MyLogger::error("Exception in list_directory: " + std::string(e.what()));
        res.set_content(R"({"error": "Internal server error"})", "application/json");
    }
}

void create_file(const httplib::Request &req, httplib::Response &res)
{
    MyLogger::info("Received file creation request");

    std::string userID;
    if (!Authentication::authenticate_request(req, res, userID))
    {
        MyLogger::warning("Authentication failed during file creation");
        if (res.body.empty())
        {
            res.status = 401;
            res.set_content(R"({"error": "Authentication failed"})", "application/json");
        }
        return;
    }

    try
    {
        json body_json = json::parse(req.body);
        if (!body_json.contains("path"))
        {
            res.status = 400;
            MyLogger::warning("File creation failed: Missing file path in request body");
            res.set_content(R"({"error": "Missing path"})", "application/json");
            return;
        }

        if (!body_json.contains("device_id"))
        {
            res.status = 400;
            MyLogger::warning("File creation failed: Missing device_id in request body");
            res.set_content(R"({"error": "Missing device_id"})", "application/json");
            return;
        }
        std::string device_id = body_json["device_id"];

        std::string file_path = body_json["path"];
        std::string key = userID + ":" + file_path;
        MyLogger::debug("Processing file creation for key: " + key);

        json existing_metadata;
        auto get_result = Database_handler::get_directory_metadata(key);
        if (get_result.success)
        {
            // If the file is tombstoned, we can proceed to create it again
            // not doing this

            MyLogger::warning("File already exists: " + key);
            res.status = 200;
            res.set_content(R"({"success": "File already exists"})", "application/json");
            return;
        }

        size_t last_slash = file_path.find_last_of('/');
        if (last_slash == std::string::npos)
        {
            res.status = 400;
            MyLogger::warning("File creation failed: Invalid file path (no parent directory) for key: " + key);
            res.set_content(R"({"error": "Invalid file path"})", "application/json");
            return;
        }

        std::string parent_dir = file_path.substr(0, last_slash);
        std::string parent_key = userID + ":" + parent_dir;
        json parent_metadata;
        MyLogger::debug("Fetching parent directory metadata for key: " + parent_key);

        auto parent_res = Database_handler::get_directory_metadata(parent_key);
        if (!parent_res.success)
        {
            res.status = 404;
            MyLogger::warning("File creation failed: Parent directory not found for key: " + parent_key + " | Error: " + parent_res.err);
            res.set_content(R"({"error": "Parent directory not found"})", "application/json");
            return;
        }

        try
        {
            parent_metadata = json::parse(parent_res.value);
            if (parent_metadata.is_string())
            {
                try
                {
                    parent_metadata = json::parse(parent_metadata.get<std::string>());
                }
                catch (const std::exception &e)
                {
                    MyLogger::error("Failed to parse inner metadata for parent directory key: " + parent_key + " | Exception: " + std::string(e.what()));
                    res.status = 500;
                    res.set_content(R"({"error": "Failed to parse inner metadata"})", "application/json");
                    return;
                }
            }
        }
        catch (const std::exception &e)
        {
            res.status = 500;
            MyLogger::error("Failed to parse parent directory metadata for key: " + parent_key + " | Exception: " + std::string(e.what()));
            res.set_content(R"({"error": "Failed to parse parent directory metadata"})", "application/json");
            return;
        }

        // Debug print the metadata
        MyLogger::debug("parent metadata: " + parent_metadata.dump());

        // Check if parent directory is tombstoned
        // not doing this

        // Check for duplicate file : already done at start

        MyLogger::info("Creating file: " + key);

        json file_meta = {
            {"owner", userID},
            {"timestamp", std::chrono::system_clock::to_time_t(std::chrono::system_clock::now())},
            {"parent_dir", parent_dir},
            {"filename", file_path},
            {"size", 0}, // any use??
            {"version", 1}};

        // Update parent metadata with new file entry.
        auto &files = parent_metadata["files"];
        files.push_back(file_path);
        MyLogger::debug("Updated parent metadata with new file: " + file_path);

        auto file_ok = Database_handler::set_directory_metadata(key, file_meta.dump());
        auto parent_ok = Database_handler::set_directory_metadata(parent_key, parent_metadata.dump());
        if (!file_ok.success || !parent_ok.success)
        {
            res.status = 500;
            MyLogger::error("Failed to update KV store for file key: " + key + " or parent key: " + parent_key);
            res.set_content(R"({"error": "Failed to store file metadata"})", "application/json");
            return;
        }

        res.status = 200;
        res.set_content(R"({"message": "File created", "metadata": )" + file_meta.dump() + "}", "application/json");
        MyLogger::info("File created successfully for key: " + key);

        // Notify the notification server about the new file.
        json notification_payload = {
            {"type", "FILE+"},
            {"user_id", userID},
            {"path", file_path},
            {"device_id", device_id}};
        MyLogger::debug("Broadcasting notification for new file: " + file_path);
        Initiation::broadcaster->broadcast(notification_payload);
    }
    catch (const std::exception &e)
    {
        res.status = 500;
        MyLogger::error("Exception in create_file: " + std::string(e.what()));
        res.set_content(R"({"error": "Internal server error"})", "application/json");
    }
}

void delete_path(const httplib::Request &req, httplib::Response &res)
{
    MyLogger::info("Received delete path request");
    std::string userID;
    if (!Authentication::authenticate_request(req, res, userID))
        return;

    json body;
    try
    {
        body = json::parse(req.body);
    }
    catch (const std::exception &e)
    {
        MyLogger::error("Failed to parse JSON body: " + std::string(e.what()));
        res.status = 400;
        res.set_content(R"({"error": "Invalid JSON"})", "application/json");
        return;
    }

    if (!body.contains("path"))
    {
        res.status = 400;
        res.set_content(R"({"error": "Missing path"})", "application/json");
        return;
    }

    if (!body.contains("device_id"))
    {
        res.status = 400;
        MyLogger::warning("Delete request failed: Missing device_id in request body");
        res.set_content(R"({"error": "Missing device_id"})", "application/json");
        return;
    }
    std::string device_id = body["device_id"];

    std::string path = body["path"];
    std::string key = userID + ":" + path;
    size_t slash = path.find_last_of('/');
    std::string parent_dir = path.substr(0, slash);
    std::string parent_key = userID + ":" + parent_dir;

    // Is it a file? (Ends with .txt)
    bool is_file = path.size() >= 4 && path.substr(path.size() - 4) == ".txt"; // >=4 or >4 ?? doesnt really matter

    auto meta = Database_handler::get_directory_metadata(key);
    if (!meta.success)
    {
        res.status = 404;
        res.set_content(R"({"error": "Path not found"})", "application/json");
        return;
    }

    Database_handler::delete_directory_metadata(key);

    // Update parent directory if parent exists
    auto parent_meta_res = Database_handler::get_directory_metadata(parent_key);
    if (parent_meta_res.success)
    {
        json parent_meta = json::parse(parent_meta_res.value);
        if (parent_meta.is_string())
            parent_meta = json::parse(parent_meta.get<std::string>());

        if (is_file && parent_meta["files"].is_array())
        {
            auto &files = parent_meta["files"];
            files.erase(std::remove(files.begin(), files.end(), path), files.end()); // remove the file from the parent directory
            DeletionManager::instance.enqueue(key);                                  // remove the data form the block storage

            // Notify the notification server about the new file.
            json notification_payload = {
                {"type", "FILE-"},
                {"user_id", userID},
                {"path", path},
                {"device_id", device_id}};
            MyLogger::debug("Broadcasting notification for delete file: " + path);
            Initiation::broadcaster->broadcast(notification_payload);
        }
        else if (!is_file && parent_meta["subdirectories"].is_array())
        {
            auto &dirs = parent_meta["subdirectories"];
            dirs.erase(std::remove(dirs.begin(), dirs.end(), path), dirs.end()); // remove the directory from the parent directory
            // Notify the notification server about the new file.
            json notification_payload = {
                {"type", "DIR-"},
                {"user_id", userID},
                {"path", path},
                {"device_id", device_id}};
            MyLogger::debug("Broadcasting notification for delete Directory: " + path);
            Initiation::broadcaster->broadcast(notification_payload);
        }

        Database_handler::set_directory_metadata(parent_key, parent_meta);
    }
    else
    {
        if (is_file)
        {
            MyLogger::warning("Parent directory not found for key: " + parent_key + "proceeding to deleting the file anyways");
            DeletionManager::instance.enqueue(key); // remove the data form the block storage
        }
    }

    res.set_content(R"({"message": "Path deleted"})", "application/json");
}

void get_file_endpoints(const httplib::Request &req, httplib::Response &res)
{
    MyLogger::info("Received request to get file endpoints");
    std::string userID;
    if (!Authentication::authenticate_request(req, res, userID))
        return;

    json body;
    try
    {
        body = json::parse(req.body);
    }
    catch (const std::exception &e)
    {
        MyLogger::error("Failed to parse JSON body: " + std::string(e.what()));
        res.status = 400;
        res.set_content(R"({"error": "Invalid JSON"})", "application/json");
        return;
    }

    if (!body.contains("path"))
    {
        res.status = 400;
        MyLogger::warning("Endpoint request failed: Missing file path parameter");
        res.set_content(R"({"error": "Missing file path parameter"})", "application/json");
        return;
    }

    std::string file_path = body["path"];
    std::string key = userID + ":" + file_path;
    MyLogger::debug("Fetching file endpoints for key: " + key);

    auto kv_response = Database_handler::get_directory_metadata(key);
    if (!kv_response.success)
    {
        res.status = 404;
        MyLogger::warning("File endpoints request failed: File not found for key: " + key);
        res.set_content(R"({"error": "File not found"})", "application/json");
        return;
    }

    json metadata;
    try
    {
        metadata = json::parse(kv_response.value);
        if (metadata.is_string())
            metadata = json::parse(metadata.get<std::string>());
    }
    catch (const std::exception &e)
    {
        res.status = 500;
        MyLogger::error("Failed to parse file metadata for endpoint retrieval for key: " + key + " | Exception: " + std::string(e.what()));
        res.set_content(R"({"error": "Failed to parse file metadata"})", "application/json");
        return;
    }

    // Check if the file is marked as deleted (tombstoned)
    // not doing this

    // Get the block server endpoints for this file
    auto &block_servers = Database_handler::select_block_server_group(key);
    json response_json = {{"endpoints", block_servers}}; // check this

    res.status = 200;
    res.set_content(response_json.dump(), "application/json");
    MyLogger::info("Returned file endpoints for key: " + key);
}

void block_server_confirmation(const httplib::Request &req, httplib::Response &res)
{
    MyLogger::info("Received block server confirmation request");
    try
    {
        json body_json = json::parse(req.body);
        if (!body_json.contains("path") || !body_json.contains("user_id") || !body_json.contains("device_id"))
        {
            res.status = 400;
            MyLogger::warning("Block server confirmation failed: Missing 'path' or 'user_id' or 'device_id' in request body");
            res.set_content(R"({"error": "Missing path or user_id"})", "application/json");
            return;
        }

        std::string file_path = body_json["path"];
        std::string userID = body_json["user_id"];
        std::string device_id = body_json["device_id"];
        MyLogger::debug("Processing block server confirmation for file: " + file_path + " by user: " + userID);

        json notification_payload = {
            {"type", "FILE~"},
            {"user_id", userID},
            {"path", file_path},
            {"device_id", device_id}};
        Initiation::broadcaster->broadcast(notification_payload);
        MyLogger::info("Block server confirmation processed for file: " + file_path);
        res.status = 200;
        res.set_content(R"({"message": "Block server confirmation received"})", "application/json");
    }
    catch (const std::exception &e)
    {
        res.status = 500;
        MyLogger::error("Exception in block_server_confirmation: " + std::string(e.what()));
        res.set_content(R"({"error": "Internal server error"})", "application/json");
    }
}

std::atomic<bool> server_running(true);
httplib::Server *global_server = nullptr;

void shutdown_server(int)
{
    if (server_running && global_server)
    {
        MyLogger::info("Received shutdown signal. Stopping server...");
        server_running = false;
        global_server->stop();
        MyLogger::info("Server stopped successfully");
    }
    exit(0);
}

int main(int argc, char *argv[])
{
    signal(SIGINT, shutdown_server);
    signal(SIGTERM, shutdown_server);

    httplib::Server svr;
    global_server = &svr;
    svr.set_logger([](const auto &req, const auto &res)
                   { std::cout << "Request: " << req.method << " " << req.path << std::endl; });
    MyLogger::info("Initializing server using configuration file: config/server_config.json");
    if (argc < 2)
    {
        std::cerr << "Usage: " << argv[0] << " <server_config_path>" << std::endl;
        return 1;
    }
    Initiation::initialize(argv[1]);

    // Routing definitions
    svr.Post("/create-directory", create_directory);
    svr.Post("/list-directory", list_directory);
    svr.Post("/create-file", create_file);
    svr.Post("/get-file-endpoints", get_file_endpoints);
    svr.Post("/block-server-confirmation", block_server_confirmation);
    svr.Post("/delete", delete_path);
    // Instantiate the DeletionManager to start the background thread
    /*
        instance of deletion manager is already created in deletion_manager.cpp
        deletion manager is a singleton class
    */

    MyLogger::info("Server started successfully at IP: " + Initiation::server_ip +
                   " Port: " + std::to_string(Initiation::server_port));
    if (!svr.listen(Initiation::server_ip, Initiation::server_port))
    {
        MyLogger::error("Failed to start server. Check IP/port binding. Server IP: " +
                        Initiation::server_ip + " | Server Port: " + std::to_string(Initiation::server_port));
        perror("Error details");
        exit(1);
    }
    return 0;
}
