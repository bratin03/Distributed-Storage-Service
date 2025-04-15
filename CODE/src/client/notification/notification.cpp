#include "notification.hpp"
#include "../logger/Mylogger.hpp"
#include <iostream>
#include <chrono>
#include <thread>
#include <nlohmann/json.hpp>

namespace beast = boost::beast;
namespace http = beast::http;
namespace asio = boost::asio;
using tcp = asio::ip::tcp;
using json = nlohmann::json;

namespace notification
{

    NotificationClient::NotificationClient(
        const std::string &server_ip,
        int server_port,
        const std::string &user_id,
        std::queue<json> &notification_queue,
        std::mutex &queue_mutex,
        std::condition_variable &queue_cv)
        : server_ip_(server_ip),
          server_port_(server_port),
          user_id_(user_id),
          notification_queue_(notification_queue),
          queue_mutex_(queue_mutex),
          cv_(queue_cv),
          running_(false) {}

    NotificationClient::~NotificationClient()
    {
        stop();
    }

    void NotificationClient::start()
    {
        if (running_)
            return;
        MyLogger::info("Starting NotificationClient for user: " + user_id_);
        running_ = true;
        client_thread_ = std::thread(&NotificationClient::run, this);
    }

    void NotificationClient::stop()
    {
        if (!running_)
            return;
        MyLogger::info("Stopping NotificationClient for user: " + user_id_);
        running_ = false;
        if (client_thread_.joinable())
        {
            client_thread_.join();
        }
    }

    void NotificationClient::run()
    {
        while (running_)
        {
            try
            {
                poll_notifications();
            }
            catch (std::exception &e)
            {
                MyLogger::error("Notification client error: " + std::string(e.what()));
                // Add a small delay before reconnecting
                std::this_thread::sleep_for(std::chrono::seconds(1000));
            }
        }
    }

    void NotificationClient::poll_notifications()
    {
        asio::io_context ioc;
        tcp::resolver resolver(ioc);
        auto const results = resolver.resolve(server_ip_, std::to_string(server_port_));
        tcp::socket socket(ioc);
        asio::connect(socket, results.begin(), results.end());

        // Form the HTTP GET request for long polling.
        std::string target = "/subscribe?userid=" + user_id_;
        http::request<http::empty_body> req{http::verb::get, target, 11};
        req.set(http::field::host, server_ip_);
        req.set(http::field::user_agent, BOOST_BEAST_VERSION_STRING);

        // Send the HTTP request.
        http::write(socket, req);

        // This buffer is used for reading and must be persisted.
        beast::flat_buffer buffer;
        http::response<http::string_body> res;
        http::read(socket, buffer, res);

        // Process the response.
        try
        {
            // Parse the response body as JSON.
            json notification = json::parse(res.body());
            MyLogger::info("Received notification for user " + user_id_ + ": " + notification.dump(2));

            // Push the notification to the queue.
            {
                std::lock_guard<std::mutex> lock(queue_mutex_);
                notification_queue_.push(notification);
                cv_.notify_one(); // Notify the waiting thread that a new notification is available.
            }
        }
        catch (json::parse_error &e)
        {
            MyLogger::error("Failed to parse notification: " + std::string(e.what()));
            MyLogger::error("Response body: " + res.body());
        }

        // Close the connection.
        boost::system::error_code ec;
        socket.shutdown(tcp::socket::shutdown_both, ec);
        if (ec)
        {
            MyLogger::error("Socket shutdown error: " + ec.message());
        }
    }
}