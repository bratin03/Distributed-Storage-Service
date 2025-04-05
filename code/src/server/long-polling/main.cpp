#include "notification_server.hpp"
#include <boost/asio.hpp>
#include <nlohmann/json.hpp>
#include <fstream>
#include <iostream>
#include <thread>

using json = nlohmann::json;
namespace asio = boost::asio;
using tcp = asio::ip::tcp;

int main() {
    try {
        // Load configuration from config.json.
        std::ifstream ifs("config.json");
        if (!ifs) {
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
        std::thread notifier([&server, &user_id]() {
            int count = 1 ; 
            while(count++ ) {
                if(count%2 == 0) {
                    user_id = "1"; 
                }
                else {
                    user_id = "2"; 
                }
                
                std::this_thread::sleep_for(std::chrono::seconds(2));
                std::cout << "Broadcasting notification to user " << user_id << "\n";
                json notification = {{"message", "New notification for user " + user_id}};

                server.broadcastNotification(user_id,notification.dump());
            }
        });

        ioc.run();
        notifier.join();
    } catch (std::exception &e) {
        std::cerr << "Error: " << e.what() << "\n";
    }
    return 0;
}
