#include <iostream>
#include "http_listener.hpp"
#include "notification_server.hpp"  // Assuming your NotificationServer is defined here.
#include <boost/asio.hpp>
#include <nlohmann/json.hpp>
#include <fstream>
#include <thread>

using json = nlohmann::json;
namespace asio = boost::asio;
using tcp = asio::ip::tcp;

int main(int argc, char *argv[])
{
    std::string config_file = "config.json";
    if (argc > 1)
        config_file = argv[1];

    // Load configuration.
    std::ifstream ifs(config_file);
    if (!ifs)
    {
        std::cerr << "Failed to open config.json\n";
        return 1;
    }
    json config;
    ifs >> config;

    std::string ip = config.value("server_ip", "0.0.0.0");
    int notification_port = config.value("server_port", 8080);
    int http_port = config.value("http_port", 8081);
    int timeout_seconds = config.value("timeout_seconds", 10);
    std::string user_id = config.value("user_id", "default");

    asio::io_context ioc;

    // Create and run the notification server.
    notification::NotificationServer notifServer(ioc, ip, static_cast<unsigned short>(notification_port), timeout_seconds);
    notifServer.run();

    // Lambda that handles the HTTP request by parsing the JSON
    // and forwarding it to the broadcast API.
    auto httpRequestHandler = [&notifServer](const std::string &body) {
        try {
            auto parsed = json::parse(body);
            std::string target_user = parsed.value("user_id", "default");
            std::string message = parsed.value("message", "No message provided");

            // Construct a notification payload.
            json notification = {
                {"message", message},
                {"timestamp", std::time(nullptr)}
            };

            // Forward the notification using the broadcast API.
            notifServer.broadcastNotification(target_user, notification.dump());
            std::cout << "Broadcasted notification for user " << target_user << "\n";
        }
        catch (const std::exception &e) {
            std::cerr << "Error parsing HTTP request body: " << e.what() << "\n";
        }
    };

    // Instantiate the HTTP listener library.
    http_listener::HttpListener httpListener(ioc, ip, static_cast<unsigned short>(http_port), httpRequestHandler);
    httpListener.run();
    ioc.run();

    return 0;
}
