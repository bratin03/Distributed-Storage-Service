#ifndef ASYNC_BROADCASTER_HPP
#define ASYNC_BROADCASTER_HPP

#include "logger/Mylogger.h" // Optional: your custom logger. Replace with std::cout/cerr if not available.
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>
#include <boost/asio.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>
#include <memory>
#include <iostream>
#include <functional>

namespace async_broadcast {

// Simple structure holding the server IP and port.
struct Server {
    std::string ip;
    unsigned short port;
};

/// Class for broadcasting a JSON message concurrently to multiple servers.
class AsyncBroadcaster {
public:
    /// Constructor takes a list of servers.
    explicit AsyncBroadcaster(const std::vector<Server>& servers)
        : servers_(servers)
    {}

    /// Broadcast the given JSON message to all servers in the list.
    /// This function will block until all asynchronous operations are completed.
    void broadcast(const nlohmann::json& message) {
        // Create an io_context to run asynchronous operations.
        boost::asio::io_context ioc;

        // Create a session for each server.
        for (const auto& server : servers_) {
            std::make_shared<Session>(ioc, server, message)->run();
        }

        // Run all asynchronous operations.
        ioc.run();
    }

private:
    std::vector<Server> servers_;

    // Internal class representing the asynchronous session for each server.
    class Session : public std::enable_shared_from_this<Session> {
    public:
        Session(boost::asio::io_context& ioc,
                const Server& server,
                const nlohmann::json& message)
            : resolver_(ioc),
              stream_(ioc),
              server_(server),
              message_(message)
        {}

        // Start the asynchronous session.
        void run() {
            MyLogger::info("Starting session for server " + server_.ip + ":" + std::to_string(server_.port));
            resolver_.async_resolve(
                server_.ip,
                std::to_string(server_.port),
                std::bind(&Session::on_resolve,
                          shared_from_this(),
                          std::placeholders::_1,
                          std::placeholders::_2));
        }

    private:
        using tcp = boost::asio::ip::tcp;
        tcp::resolver resolver_;
        boost::beast::tcp_stream stream_;
        Server server_;
        nlohmann::json message_;
        boost::beast::flat_buffer buffer_;
        boost::beast::http::request<boost::beast::http::string_body> req_;
        boost::beast::http::response<boost::beast::http::dynamic_body> res_;

        // Callback for DNS resolution.
        void on_resolve(boost::system::error_code ec, tcp::resolver::results_type results) {
            if (ec) {
                MyLogger::error("Resolve error for " + server_.ip + ": " + ec.message());
                return;
            }
            stream_.async_connect(
                results,
                std::bind(&Session::on_connect,
                          shared_from_this(),
                          std::placeholders::_1));
        }

        // Callback once connected.
        void on_connect(boost::system::error_code ec) {
            if (ec) {
                MyLogger::error("Connect error for " + server_.ip + ": " + ec.message());
                return;
            }
            MyLogger::info("Connected to " + server_.ip + ":" + std::to_string(server_.port));

            // Prepare the HTTP POST request.
            req_.version(11);
            req_.method(boost::beast::http::verb::post);
            req_.target("/broadcast");
            req_.set(boost::beast::http::field::host, server_.ip);
            req_.set(boost::beast::http::field::user_agent, BOOST_BEAST_VERSION_STRING);
            req_.set(boost::beast::http::field::content_type, "application/json");
            req_.body() = message_.dump();
            req_.prepare_payload();

            boost::beast::http::async_write(
                stream_,
                req_,
                std::bind(&Session::on_write,
                          shared_from_this(),
                          std::placeholders::_1,
                          std::placeholders::_2));
        }

        // Callback after sending the request.
        void on_write(boost::system::error_code ec, std::size_t bytes_transferred) {
            boost::ignore_unused(bytes_transferred);
            if (ec) {
                MyLogger::error("Write error for " + server_.ip + ": " + ec.message());
                return;
            }
            MyLogger::info("HTTP POST request sent to " + server_.ip);

            boost::beast::http::async_read(
                stream_,
                buffer_,
                res_,
                std::bind(&Session::on_read,
                          shared_from_this(),
                          std::placeholders::_1,
                          std::placeholders::_2));
        }

        // Callback after reading the response.
        void on_read(boost::system::error_code ec, std::size_t bytes_transferred) {
            boost::ignore_unused(bytes_transferred);
            if (ec) {
                MyLogger::error("Read error for " + server_.ip + ": " + ec.message());
                return;
            }

            auto bodyStr = boost::beast::buffers_to_string(res_.body().data());
            MyLogger::info("Received response from " + server_.ip + ": " + bodyStr);

            // Gracefully close the connection.
            boost::system::error_code shutdown_ec;
            stream_.socket().shutdown(tcp::socket::shutdown_both, shutdown_ec);
            if (shutdown_ec && shutdown_ec != boost::beast::errc::not_connected) {
                MyLogger::error("Shutdown error for " + server_.ip + ": " + shutdown_ec.message());
                return;
            }
            MyLogger::info("Connection closed gracefully for " + server_.ip);
        }
    }; // End class Session

}; // End class AsyncBroadcaster

} // End namespace async_broadcast

#endif // ASYNC_BROADCASTER_HPP
