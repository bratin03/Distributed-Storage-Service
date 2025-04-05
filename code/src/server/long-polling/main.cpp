#include "notification_server.hpp"
#include <boost/asio.hpp>
#include <nlohmann/json.hpp>
#include <fstream>
#include <iostream>
#include <thread>

using json = nlohmann::json;
namespace asio = boost::asio;
using tcp = asio::ip::tcp;

int main(int argc, char *argv[])
{
    std::string config_file = "config.json";
    if (argc > 1)
    {
        config_file = argv[1];
    }

    // Seed the random number generator.
    std::srand(static_cast<unsigned int>(std::time(nullptr)));

    try
    {
        // Load configuration from config.json.
        std::ifstream ifs(config_file);
        if (!ifs)
        {
            std::cerr << "Failed to open config.json\n";
            return 1;
        }
        json config;
        ifs >> config;

        std::string ip = config.value("server_ip", "0.0.0.0");
        int port = config.value("server_port", 8080);
        int timeout_seconds = config.value("timeout_seconds", 10);
        std::string user_id = config.value("user_id", "default");

        asio::io_context ioc;

        // Create and run the notification server using only IP, port, and timeout.
        notification::NotificationServer server(ioc, ip, static_cast<unsigned short>(port), timeout_seconds);
        server.run();

        // For demonstration, simulate an external event that broadcasts a notification after 5 seconds.
        std::thread notifier([&server, &user_id,&ip]()
                             {
            int count = 1 ; 
            while(count++ ) {
                if(count%2 == 0) {
                    user_id = "1"; 
                }
                else {
                    user_id = "2"; 
                }
                
                json notification = {{"message", "New notification for user " + user_id },
                                     {"timestamp", std::time(nullptr)},
                                     {"client_ip", ip}};

                // Simulate a delay before sending the notification.
                std::this_thread::sleep_for(std::chrono::seconds(2));
                server.broadcastNotification(user_id,notification.dump());
            } });

        ioc.run();
        notifier.join();
    }
    catch (std::exception &e)
    {
        std::cerr << "Error: " << e.what() << "\n";
    }
    return 0;
}
