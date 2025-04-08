#include <iostream>
#include <vector>
#include <string>
#define CPPHTTPLIB_HEADER_ONLY
#include "../../utils/libraries/cpp-httplib/httplib.h"
#include <nlohmann/json.hpp>
#include <optional>

using json = nlohmann::json;

// Define a simple structure to hold server information.
struct Server
{
    std::string ip;
    int port;
};

std::optional<std::string> get_token(const std::string &auth_ip, int auth_port, const std::string &user, const std::string &pass)
{
    httplib::Client auth_cli(auth_ip, auth_port);
    json login_payload = {
        {"userID", user},
        {"password", pass}};

    auto res = auth_cli.Post("/login", login_payload.dump(), "application/json");
    if (!res || res->status != 200)
    {
        std::cerr << "Login failed: ";
        if (res)
            std::cerr << res->status << " - " << res->body << std::endl;
        else
            std::cerr << "No response" << std::endl;
        return std::nullopt;
    }

    json response_json = json::parse(res->body);
    if (!response_json.contains("token"))
    {
        std::cerr << "Login response missing token" << std::endl;
        return std::nullopt;
    }

    return response_json["token"];
}

void test_create_directory(const std::vector<Server> &servers, const std::string &token, const std::string & dir_path )
{
    json dir_payload = {
        {"path", dir_path}};

    for (const auto &server : servers)
    {
        httplib::Client cli(server.ip, server.port);
        httplib::Headers headers = {
            {"Authorization", "Bearer " + token}};

        std::cout << "\nCalling /create-directory on " << server.ip << ":" << server.port << std::endl;
        auto res = cli.Post("/create-directory", headers, dir_payload.dump(), "application/json");

        if (res)
        {
            std::cout << "Status: " << res->status << std::endl;
            std::cout << "Response: " << res->body << std::endl;
        }
        else
        {
            std::cerr << "No response from server: " << server.ip << ":" << server.port << std::endl;
        }
    }
}


void test_create_file(const std::vector<Server>& storage_servers, const std::string& token, const std::string& file_path) {
    // Construct the JSON payload with the given file path.
    json payload;
    payload["path"] = file_path;

    // Loop over each storage server.
    for (const auto& server : storage_servers) {
        // Create an HTTP client instance for the current server.
        httplib::Client cli(server.ip, server.port);
        
        // Set up the authorization header using the provided token.
        httplib::Headers headers = {
            {"Authorization", "Bearer " + token}
        };

        std::cout << "-----------------------------" << std::endl;
        std::cout << "Sending /create_file request to " << server.ip << ":" << server.port << std::endl;

        // Send the POST request to /create_file.
        auto res = cli.Post("/create-file", headers, payload.dump(), "application/json");

        // Check and print out the HTTP status code and response.
        if (res) {
            std::cout << "Status: " << res->status << std::endl;
            std::cout << "Response: " << res->body << std::endl;
        } else {
            std::cerr << "Error: No response from server " << server.ip << ":" << server.port << std::endl;
        }
    }
    std::cout << "-----------------------------" << std::endl;
}

int main()
{
    // List of servers to test.
    std::vector<Server> servers = {
        {"127.0.0.1", 10000},
    };

    // Construct the JSON payload for signup.
    json payload;
    payload["username"] = "lord";
    payload["password"] = "testpassword"; // In production, this should be a hashed password

    // Iterate through each server, send the POST request, and print results.
    for (const auto &server : servers)
    {
        // Create an HTTP client for this server.
        httplib::Client cli(server.ip, server.port);

        // Send the POST request to the /signup endpoint.
        auto res = cli.Post("/signup", payload.dump(), "application/json");

        std::cout << "-------------------------------------" << std::endl;
        std::cout << "Testing server at " << server.ip << ":" << server.port << std::endl;

        if (res)
        {
            // Print status and response body.
            if (res->status == 200)
            {
                std::cout << "Signup successful: " << res->body << std::endl;
            }
            else
            {
                std::cout << "Signup failed (" << res->status << "): " << res->body << std::endl;
            }
        }
        else
        {
            std::cerr << "Error: No response from server at " << server.ip << ":" << server.port << std::endl;
        }
    }
    std::cout << "-------------------------------------" << std::endl;

    std::vector<Server> auth_servers = {
        {"127.0.0.1", 30000}};

    // Construct the JSON payload for login.
    payload["userID"] = "lord";
    payload["password"] = "testpassword"; // Remember, use hashed passwords in production.

    json auth_token;
    // Iterate over each server and send the POST request.
    for (const auto &server : auth_servers)
    {
        // Create an HTTP client for the current server.
        httplib::Client cli(server.ip, server.port);

        // Send the POST request to the /login endpoint with the JSON payload.
        auto res = cli.Post("/login", payload.dump(), "application/json");

        std::cout << "-------------------------------------" << std::endl;
        std::cout << "Testing login server at " << server.ip << ":" << server.port << std::endl;

        if (res)
        {
            // Print response status and body.
            std::cout << "Status: " << res->status << std::endl;
            std::cout << "Response: " << res->body << std::endl;
            auth_token = json::parse(res->body);
            if (!auth_token.contains("token"))
            {
                std::cerr << "Login response missing token" << std::endl;
                return 0;
            }
        }
        else
        {
            // Notify when no response is received.
            std::cerr << "Error: No response from server at " << server.ip << ":" << server.port << std::endl;
        }
    }
    std::cout << "-------------------------------------" << std::endl;
    std::cout << "-------------------------------------" << std::endl;

    std::string token = auth_token["token"];

    std::vector<Server> storage_servers = {
        {"127.0.0.3", 30000}};

    // test_create_directory(storage_servers, token,"dropbox/testdir");
    // test_create_directory(storage_servers, token,"dropbox/testdir");
    // test_create_directory(storage_servers, token,"/testdir");

    std::cout << "-------------------------------------" << std::endl;

    test_create_file(storage_servers, token, "dropbox/testdir/testfile.txt");
    // test_create_file(storage_servers, token, "testdir/testfile.txt");

    return 0;
}
