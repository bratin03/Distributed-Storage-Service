#define CPPHTTPLIB_OPENSSL_SUPPORT

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
        MyLogger::debug("Selecting metastorage group for key: " + key + " | Group index: " + std::to_string(idx));
        return Initiation::metastorage_groups[idx];
    }

    json &select_block_server_group(const std::string &key)
    {
        static std::hash<std::string> hasher;
        size_t idx = hasher(key) % Initiation::blockserver_lists.size();
        MyLogger::debug("Selecting block server group for key: " + key + " | Group index: " + std::to_string(idx));
        return Initiation::blockserver_lists[idx];
    }

    distributed_KV::Response get_directory_metadata(const std::string &key)
    {
        MyLogger::debug("Getting directory metadata for key: " + key);
        auto &servers = select_metastorage_group(key);
        distributed_KV::Response res = distributed_KV::get(servers, key);
        if (!res.success)
        {
            MyLogger::warning("KV GET failed for key: " + key + " | Error: " + res.err);
            return res;
        }
        MyLogger::info("KV GET successful for key: " + key);
        return res;
    }

    distributed_KV::Response set_directory_metadata(const std::string &key, const json &metadata)
    {
        MyLogger::debug("Setting directory metadata for key: " + key + " | Metadata: " + metadata.dump());
        std::string value = metadata.dump();
        distributed_KV::Response res = distributed_KV::set(select_metastorage_group(key), key, value);
        if (!res.success)
            MyLogger::warning("KV SET failed for key: " + key + " | Error: " + res.err);
        else
            MyLogger::info("KV SET successful for key: " + key);
        return res;
    }

    distributed_KV::Response delete_directory_metadata(const std::string &key)
    {
        MyLogger::debug("Deleting directory metadata for key: " + key);
        distributed_KV::Response res = distributed_KV::del(select_metastorage_group(key), key);
        if (!res.success)
            MyLogger::warning("KV DELETE failed for key: " + key + " | Error: " + res.err);
        else
            MyLogger::info("KV DELETE successful for key: " + key);
        return res;
    }
}

void create_directory(const httplib::Request &req, httplib::Response &res)
{
    MyLogger::info("Received directory creation request");
    std::string userID;
    if (!Authentication::authenticate_request(req, res, userID))
    {
        MyLogger::warning("Authentication failed during directory creation");
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

        std::string dir_id = body_json["path"];
        std::string key = userID + ":" + dir_id;
        MyLogger::debug("Processing directory creation for key: " + key);

        json existing_metadata;
        auto get_result = Database_handler::get_directory_metadata(key);
        if (get_result.success)
        {
            MyLogger::warning("Directory already exists for key: " + key);
            try
            {
                existing_metadata = json::parse(get_result.value);
            }
            catch (const std::exception &e)
            {
                res.status = 500;
                MyLogger::error("Failed to parse existing directory metadata for key: " + key + " | Exception: " + std::string(e.what()));
                res.set_content(R"({"error": "Failed to parse existing directory metadata"})", "application/json");
                return;
            }
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
            MyLogger::debug("Parent metadata: " + parent_metadata.dump());
        }
        catch (const std::exception &e)
        {
            res.status = 500;
            MyLogger::error("Failed to parse parent directory metadata for key: " + parent_key + " | Exception: " + std::string(e.what()));
            res.set_content(R"({"error": "Failed to parse parent directory metadata"})", "application/json");
            return;
        }

        // New directory metadata including owner, timestamp, empty subdirectories, files, and not deleted.
        json new_metadata = {
            {"owner", userID},
            {"timestamp", std::chrono::system_clock::to_time_t(std::chrono::system_clock::now())},
            {"subdirectories", json::array()},
            {"files", json::array()},
            {"deleted", false}};

        auto set_result = Database_handler::set_directory_metadata(key, new_metadata);
        if (!set_result.success)
        {
            res.status = 500;
            MyLogger::error("Failed to write new directory metadata for key: " + key + " | Error: " + set_result.err);
            res.set_content(R"({"error": "Failed to create directory"})", "application/json");
            return;
        }

        // Update parent directory metadata to include the new directory.
        if (!parent_key.empty())
        {
            std::string dir_name = dir_id.substr(last_slash + 1);
            auto &subdirs = parent_metadata["subdirectories"];
            subdirs.push_back(dir_name);
            MyLogger::debug("Updated parent metadata with new subdirectory: " + dir_name);

            auto update_parent_result = Database_handler::set_directory_metadata(parent_key, parent_metadata);
            if (!update_parent_result.success)
            {
                MyLogger::warning("Directory created but failed to update parent metadata for key: " + parent_key + " | Error: " + update_parent_result.err);
                // Optional: Implement rollback or further error handling if needed.
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
            {"path", dir_id}};

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

        std::string file_path = body_json["path"];
        std::string key = userID + ":" + file_path;
        MyLogger::debug("Processing file creation for key: " + key);

        json existing_metadata;
        auto get_result = Database_handler::get_directory_metadata(key);
        if (get_result.success)
        {
            MyLogger::warning("File already exists for key: " + key);
            try
            {
                existing_metadata = json::parse(get_result.value);
            }
            catch (const std::exception &e)
            {
                res.status = 500;
                MyLogger::error("Failed to parse existing file metadata for key: " + key + " | Exception: " + std::string(e.what()));
                res.set_content(R"({"error": "Failed to parse existing file metadata"})", "application/json");
                return;
            }
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
        std::string filename = file_path.substr(last_slash + 1);
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

        MyLogger::debug("Parent metadata: " + parent_metadata.dump());
        // Check for duplicate file in the parent's file list.
        if (parent_metadata["files"].contains(filename))
        {
            res.status = 409;
            MyLogger::warning("File already exists in parent directory: " + filename);
            res.set_content(R"({"error": "File already exists"})", "application/json");
            return;
        }

        MyLogger::info("Creating file for key: " + key);

        json file_meta = {
            {"parent_dir", parent_dir},
            {"filename", filename},
            {"owner", userID},
            {"timestamp", std::chrono::system_clock::to_time_t(std::chrono::system_clock::now())},
            {"size", 0},
            {"version", 1},
            {"deleted", false}};

        // Update parent metadata with new file entry.
        auto &files = parent_metadata["files"];
        files.push_back(filename);
        MyLogger::debug("Updated parent metadata with new file: " + filename);

        auto file_ok = Database_handler::set_directory_metadata(key, file_meta.dump());
        auto parent_ok = Database_handler::set_directory_metadata(parent_key, parent_metadata.dump());
        if (!file_ok.success || !parent_ok.success)
        {
            res.status = 500;
            MyLogger::error("Failed to update KV store for file key: " + key + " or parent key: " + parent_key);
            res.set_content(R"({"error": "Failed to store file metadata"})", "application/json");
            return;
        }

        res.set_content(R"({"message": "File created", "metadata": )" + file_meta.dump() + "}", "application/json");
        MyLogger::info("File created successfully for key: " + key);

        // Notify the notification server about the new file.
        json notification_payload = {
            {"type", "FILE+"},
            {"user_id", userID},
            {"path", file_path}};

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

void update_file(const httplib::Request &req, httplib::Response &res)
{
    MyLogger::info("Received file update request");

    std::string userID;
    if (!Authentication::authenticate_request(req, res, userID))
    {
        MyLogger::warning("Authentication failed during file update");
        return;
    }

    try
    {
        json body_json = json::parse(req.body);
        if (!body_json.contains("path") || !body_json.contains("version"))
        {
            res.status = 400;
            MyLogger::warning("File update failed: Missing path or version in request body");
            res.set_content(R"({"error": "Missing path or version"})", "application/json");
            return;
        }

        std::string file_path = body_json["path"];
        int client_version = body_json["version"];
        std::string key = userID + ":" + file_path;
        MyLogger::debug("Processing file update for key: " + key + " | Client version: " + std::to_string(client_version));

        auto kv_response = Database_handler::get_directory_metadata(key);
        if (!kv_response.success)
        {
            res.status = 404;
            MyLogger::warning("File update failed: File not found for key: " + key);
            res.set_content(R"({"error": "File not found"})", "application/json");
            return;
        }

        json metadata;
        try
        {
            metadata = json::parse(kv_response.value);
            if (metadata.is_string())
            {
                try
                {
                    metadata = json::parse(metadata.get<std::string>());
                }
                catch (const std::exception &e)
                {
                    MyLogger::error("Failed to parse inner file metadata for key: " + key + " | Exception: " + std::string(e.what()));
                    res.status = 500;
                    res.set_content(R"({"error": "Failed to parse inner metadata"})", "application/json");
                    return;
                }
            }
        }
        catch (const std::exception &e)
        {
            res.status = 500;
            MyLogger::error("Failed to parse file metadata for key: " + key + " | Exception: " + std::string(e.what()));
            res.set_content(R"({"error": "Failed to parse file metadata"})", "application/json");
            return;
        }

        MyLogger::debug("Current file metadata for key: " + key + " | Data: " + metadata.dump());
        int current_version = metadata.value("version", 1); // Default version 1 if not set

        json &block_servers = Database_handler::select_block_server_group(key);
        if (client_version == current_version)
        {
            res.status = 200;
            json response_json = {
                {"status", "ok"},
                {"servers", block_servers}};
            res.set_content(response_json.dump(), "application/json");
            MyLogger::info("File update accepted for key: " + key);
        }
        else
        {
            res.status = 409;
            json response_json = {
                {"status", "outdated"},
                {"current_version", current_version},
                {"servers", block_servers}};
            res.set_content(response_json.dump(), "application/json");
            MyLogger::warning("File update rejected due to version mismatch for key: " + key);
        }
    }
    catch (const std::exception &e)
    {
        res.status = 500;
        MyLogger::error("Exception in update_file: " + std::string(e.what()));
        res.set_content(R"({"error": "Internal server error"})", "application/json");
    }
}

void get_file_endpoints(const httplib::Request &req, httplib::Response &res)
{
    MyLogger::info("Received request to get file endpoints");
    std::string userID;
    if (!Authentication::authenticate_request(req, res, userID))
    {
        MyLogger::warning("Authentication failed during endpoint retrieval");
        return;
    }

    if (!req.has_param("path"))
    {
        res.status = 400;
        MyLogger::warning("Endpoint request failed: Missing file path parameter");
        res.set_content(R"({"error": "Missing file path parameter"})", "application/json");
        return;
    }

    std::string file_path = req.get_param_value("path");
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

    json &block_servers = Database_handler::select_block_server_group(key);
    json response_json = {{"endpoints", block_servers}};
    res.status = 200;
    res.set_content(response_json.dump(), "application/json");
    MyLogger::info("Returned file endpoints for key: " + key);
}

void block_server_confirmation(const httplib::Request &req, httplib::Response &res)
{
    MyLogger::info("Received block server confirmation request");
    json body_json = json::parse(req.body);
    if (!body_json.contains("path") || !body_json.contains("user_id"))
    {
        res.status = 400;
        MyLogger::warning("Block server confirmation failed: Missing 'path' or 'user_id' in request body");
        res.set_content(R"({"error": "Missing path or user_id"})", "application/json");
        return;
    }

    std::string file_path = body_json["path"];
    std::string userID = body_json["user_id"];
    MyLogger::debug("Processing block server confirmation for file: " + file_path + " by user: " + userID);

    json notification_payload = {
        {"type", "FILE+"},
        {"user_id", userID},
        {"path", file_path}};
    Initiation::broadcaster->broadcast(notification_payload);
    MyLogger::info("Block server confirmation processed for file: " + file_path);
}

std::atomic<bool> server_running(true);
httplib::Server *global_server = nullptr;

void shutdown_server()
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

int main()
{
    signal(SIGINT, [](int signum)
           { shutdown_server(); });
    signal(SIGTERM, [](int signum)
           { shutdown_server(); });

    httplib::Server svr;
    global_server = &svr;
    svr.set_logger([](const auto &req, const auto &res)
                   { std::cout << "Request: " << req.method << " " << req.path << std::endl; });
    MyLogger::info("Initializing server using configuration file: config/server_config.json");
    Initiation::initialize("config/server_config.json");

    // Routing definitions
    svr.Post("/create-directory", create_directory);
    svr.Post("/list-directory", list_directory);
    svr.Post("/create-file", create_file);
    svr.Post("/update-file", update_file);
    svr.Post("/get-file-endpoints", get_file_endpoints);
    svr.Post("/block-server-confirmation", block_server_confirmation);

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

// End of API definitions.
// Additional APIs (e.g., to get servers, delete, or atomic locking checks) may be added below.
