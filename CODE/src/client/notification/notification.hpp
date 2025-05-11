/*
    CS60002 - Distributed Systems
    Term Project - Spring 2025

    * Author 1: Bratin Mondal (21CS10016)
    * Author 2: Soukhin Nayek (21CS10062)
    * Author 3: Swarnabh Mandal (21CS10068)

    * Department of Computer Science and Engineering
    * Indian Institute of Technology, Kharagpur
*/

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

namespace notification
{
    class NotificationClient
    {
    public:
        NotificationClient(
            const std::string &server_ip,
            int server_port,
            const std::string &user_id,
            std::queue<nlohmann::json> &notification_queue,
            std::mutex &queue_mutex,
            std::condition_variable &cv);

        ~NotificationClient();

        // Start the notification client in a separate thread
        void start();

        // Stop the notification client
        void stop();

    private:
        std::string server_ip_;
        int server_port_;
        std::string user_id_;
        std::queue<nlohmann::json> &notification_queue_;
        std::mutex &queue_mutex_;
        std::thread client_thread_;
        std::condition_variable &cv_;
        std::atomic<bool> running_;

        void run();
        void poll_notifications();
    };
}