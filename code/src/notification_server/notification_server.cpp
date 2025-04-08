#include "logger/Mylogger.h"
#include "notification_server.hpp"
#include <chrono>
#include <memory>

namespace notification
{

    using namespace std::chrono_literals;

    // ----------------------- NotificationServer Implementation -----------------------

    NotificationServer::NotificationServer(asio::io_context &ioc, const std::string &ip, unsigned short port, int timeout_seconds)
        : acceptor_(ioc, tcp::endpoint(asio::ip::make_address(ip), port)),
          timeout_seconds_(timeout_seconds)
    {
        MyLogger::info("NotificationServer created at " + ip + ":" + std::to_string(port));
    }

    void NotificationServer::run()
    {
        MyLogger::info("NotificationServer running.");
        do_accept();
    }

    void NotificationServer::do_accept()
    {
        acceptor_.async_accept(
            [this](boost::system::error_code ec, tcp::socket socket)
            {
                if (!ec)
                {
                    MyLogger::info("Accepted new connection.");
                    auto remote_endpoint = socket.remote_endpoint();
                    std::string client_ip = remote_endpoint.address().to_string();
                    unsigned short client_port = remote_endpoint.port();
                    MyLogger::info("Client connected from IP: " + client_ip + ", Port: " + std::to_string(client_port));
                    std::make_shared<Session>(std::move(socket), *this)->start();
                }
                else
                {
                    MyLogger::error("Accept error: " + ec.message());
                }
                do_accept();
            });
    }

    void NotificationServer::broadcastNotification(const std::string &user_id, const std::string &message)
    {
        std::vector<SessionPtr> sessions;
        {
            std::lock_guard<std::mutex> lock(subs_mutex_);
            auto it = subscriptions_.find(user_id);
            if (it != subscriptions_.end())
            {
                sessions = std::move(it->second);
                subscriptions_.erase(it);
            }
        }
        MyLogger::info("Broadcasting notification to user: " + user_id);
        // Send the notification to all waiting sessions.
        for (auto &session : sessions)
        {
            session->sendNotification(message);
        }
    }

    // ---------------------------- Session Implementation ----------------------------

    Session::Session(tcp::socket socket, NotificationServer &server)
        : socket_(std::move(socket)), timer_(socket_.get_executor()), server_(server)
    {
        MyLogger::info("Session created.");
    }

    void Session::start()
    {
        MyLogger::info("Session started.");
        do_read();
    }

    void Session::do_read()
    {
        auto self = shared_from_this();
        http::async_read(socket_, buffer_, req_,
                         [this, self](boost::system::error_code ec, std::size_t bytes_transferred)
                         {
                             if (!ec)
                             {
                                 MyLogger::info("HTTP request received.");
                                 handle_request();
                             }
                             else
                             {
                                 MyLogger::error("HTTP read error: " + ec.message());
                             }
                         });
    }

    void Session::handle_request()
    {
        // Assume the target is in the format: /subscribe?userid=<user_id>
        std::string target = std::string(req_.target());
        auto pos = target.find("userid=");
        if (pos != std::string::npos)
        {
            user_id_ = target.substr(pos + 7);
        }
        else
        {
            user_id_ = "default";
        }
        MyLogger::info("Session subscribed with user id: " + user_id_);

        // Add this session to the server's subscription map.
        {
            std::lock_guard<std::mutex> lock(server_.subs_mutex_);
            server_.subscriptions_[user_id_].push_back(shared_from_this());
        }

        // Start the timeout timer.
        timer_.expires_after(std::chrono::seconds(server_.timeout_seconds_));
        auto self_timer = shared_from_this();
        timer_.async_wait([this, self_timer](boost::system::error_code ec)
                          {
        if (!ec) {
            MyLogger::info("Timeout expired for user: " + user_id_);
            // Timer expired; send a timeout response ("false").
            sendTimeoutResponse();
        } });
    }

    void Session::sendNotification(const std::string &message)
    {
        // Cancel the timeout timer.
        boost::system::error_code ec;
        timer_.cancel(ec);

        MyLogger::info("Sending notification to user: " + user_id_);
        auto self = shared_from_this();
        // Allocate the response on the heap to keep it alive during async_write.
        auto res = std::make_shared<http::response<http::string_body>>(http::status::ok, req_.version());
        res->set(http::field::content_type, "text/plain");
        res->body() = message;
        res->prepare_payload();

        http::async_write(socket_, *res,
                          [this, self, res](boost::system::error_code ec, std::size_t)
                          {
                              if (ec)
                              {
                                  MyLogger::error("Error sending notification: " + ec.message());
                              }
                              socket_.shutdown(tcp::socket::shutdown_send, ec);
                          });
    }

    void Session::sendTimeoutResponse()
    {
        MyLogger::info("Sending timeout response to user: " + user_id_);
        auto self = shared_from_this();
        // Allocate the timeout response on the heap.
        auto res = std::make_shared<http::response<http::string_body>>(http::status::ok, req_.version());
        res->set(http::field::content_type, "text/plain");
        res->body() = "false";
        res->prepare_payload();

        http::async_write(socket_, *res,
                          [this, self, res](boost::system::error_code ec, std::size_t)
                          {
                              if (ec)
                              {
                                  MyLogger::error("Error sending timeout response: " + ec.message());
                              }
                              socket_.shutdown(tcp::socket::shutdown_send, ec);
                          });
    }

} // namespace notification
