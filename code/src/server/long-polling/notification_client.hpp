#pragma once

#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>
#include <boost/asio.hpp>
#include <nlohmann/json.hpp>
#include <string>
#include <queue>
#include <mutex>
#include <thread>
#include <atomic>

class NotificationClient {
public:
    NotificationClient(
        const std::string& server_ip,
        int server_port,
        const std::string& user_id,
        std::queue<nlohmann::json>& notification_queue,
        std::mutex& queue_mutex
    );

    ~NotificationClient();

    // Start the notification client in a separate thread
    void start();

    // Stop the notification client
    void stop();

private:
    std::string server_ip_;
    int server_port_;
    std::string user_id_;
    std::queue<nlohmann::json>& notification_queue_;
    std::mutex& queue_mutex_;
    std::atomic<bool> running_;
    std::thread client_thread_;

    void run();
    void poll_notifications();
};
