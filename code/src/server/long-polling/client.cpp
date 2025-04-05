#include "notification_client.hpp"
#include <iostream>
#include <queue>
#include <fstream>
#include <mutex>
#include <chrono>
#include <thread>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

int main(int argc, char *argv[])
{
    // Create a queue and a mutex for notifications
    std::queue<json> notification_queue;
    std::mutex queue_mutex;

    // Load configuration from config.json.
    std::ifstream ifs("config.json");
    if (!ifs)
    {
        std::cerr << "Failed to open config.json\n";
        return 1;
    }
    json config;
    ifs >> config;

    auto port = config.value("server_port", 8080);
    auto ip = config.value("server_ip","127.0.0.1");

    auto user_id = argv[1];

    
    // Create and start the notification client
    NotificationClient client(ip, port, user_id, notification_queue, queue_mutex);
    client.start();

    std::cout << "Notification client started. Waiting for notifications..." << std::endl;

    // Process notifications from the queue in this thread
    while (true)
    {
        bool has_notification = false;
        json notification;

        {
            std::lock_guard<std::mutex> lock(queue_mutex);
            if (!notification_queue.empty())
            {
                notification = notification_queue.front();
                notification_queue.pop();
                has_notification = true;
            }
        }

        if (has_notification)
        {
            std::cout << "Received notification: " << notification.dump(2) << std::endl;
            // Process the notification here
        }

        // Sleep a bit to avoid busy waiting
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    return 0;
}
