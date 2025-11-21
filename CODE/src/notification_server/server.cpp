/*
    CS60002 - Distributed Systems
    Term Project - Spring 2025

    * Author 1: Bratin Mondal (21CS10016)
    * Author 2: Soukhin Nayek (21CS10062)
    * Author 3: Swarnabh Mandal (21CS10068)

    * Department of Computer Science and Engineering
    * Indian Institute of Technology, Kharagpur
*/

#include <boost/asio.hpp>
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>
#include <thread>

#include "http_listener.hpp"
#include "Mylogger.hpp"        // Custom logger header.
#include "notification_server.hpp"  // Assuming your NotificationServer is defined here.

using json = nlohmann::json;
namespace asio = boost::asio;
using tcp = asio::ip::tcp;

int main(int argc, char *argv[]) {
    std::string config_file = "config.json";
    if (argc > 1) config_file = argv[1];

    // Load configuration.
    std::ifstream ifs(config_file);
    if (!ifs) {
        MyLogger::error("Failed to open " + config_file);
        return 1;
    }
    json config;
    try {
        ifs >> config;
    } catch (const std::exception &e) {
        MyLogger::error("Error parsing " + config_file + ": " + std::string(e.what()));
        return 1;
    }

    std::string ip = config.value("server_ip", "0.0.0.0");
    int notification_port = config.value("server_port", 8080);
    int http_port = config.value("http_port", 8081);
    int timeout_seconds = config.value("timeout_seconds", 10);
    std::string user_id = config.value("user_id", "default");

    MyLogger::info("Configuration loaded. Starting servers...");

    asio::io_context ioc;

    // Create and run the notification server.
    notification::NotificationServer notifServer(
        ioc, ip, static_cast<unsigned short>(notification_port), timeout_seconds);
    notifServer.run();
    MyLogger::info("Notification server is running on " + ip + ":" +
                   std::to_string(notification_port));

    // Lambda that handles the HTTP request by parsing the JSON
    // to retrieve the target user and then broadcasting the exact JSON it receives.
    auto httpRequestHandler = [&notifServer](const std::string &body) {
        try {
            auto parsed = json::parse(body);
            std::string target_user = parsed.value("user_id", "default");
            MyLogger::info("Received HTTP request to broadcast to user " + target_user);

            // Broadcast the exact JSON payload received.
            notifServer.broadcastNotification(target_user, body);
            MyLogger::info("Broadcasted notification for user " + target_user);
        } catch (const std::exception &e) {
            MyLogger::error("Error parsing HTTP request body: " + std::string(e.what()));
        }
    };

    // Instantiate the HTTP listener.
    http_listener::HttpListener httpListener(ioc, ip, static_cast<unsigned short>(http_port),
                                             httpRequestHandler);
    httpListener.run();
    MyLogger::info("HTTP Listener is running on " + ip + ":" + std::to_string(http_port));

    // Run the io_context to start handling requests.
    ioc.run();

    return 0;
}
