#include "logger/Mylogger.h" // Your custom logger.
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>
#include <boost/asio.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>
#include <memory>
#include <nlohmann/json.hpp> // JSON support

// Using declarations for Boost.Beast and Boost.Asio namespaces.
namespace beast = boost::beast;
namespace http = beast::http;
namespace asio = boost::asio;
using tcp = asio::ip::tcp;

// Define a simple structure to hold server information.
struct Server
{
    std::string ip;
    unsigned short port;
};

// Global list of servers
std::vector<Server> serverList = {
    {"127.0.0.2", 10011},
    {"127.0.0.2", 10012},
    {"127.0.0.2", 10013},
    // Add more servers as needed.
};

// Session class handles asynchronous connection and communication with one server.
class Session : public std::enable_shared_from_this<Session>
{
public:
    Session(asio::io_context &ioc, const Server &server, const nlohmann::json &message)
        : resolver_(ioc),
          stream_(ioc),
          server_(server),
          message_(message)
    {
    }

    // Start the asynchronous chain.
    void run()
    {
        MyLogger::info("Starting session for server " +
                       server_.ip + ":" + std::to_string(server_.port));
        // Begin async resolution.
        resolver_.async_resolve(
            server_.ip,
            std::to_string(server_.port),
            std::bind(
                &Session::on_resolve,
                shared_from_this(),
                std::placeholders::_1,
                std::placeholders::_2));
    }

private:
    tcp::resolver resolver_;
    beast::tcp_stream stream_;
    const Server server_;
    nlohmann::json message_;
    beast::flat_buffer buffer_;
    http::request<http::string_body> req_;
    http::response<http::dynamic_body> res_;

    // Called once the DNS resolution is complete.
    void on_resolve(beast::error_code ec, tcp::resolver::results_type results)
    {
        if (ec)
        {
            MyLogger::error("Resolve error for " + server_.ip + ": " + ec.message());
            return;
        }
        // Asynchronously connect to the resolved endpoint.
        stream_.async_connect(
            results,
            std::bind(
                &Session::on_connect,
                shared_from_this(),
                std::placeholders::_1));
    }

    // Called when connection is established.
    void on_connect(beast::error_code ec)
    {
        if (ec)
        {
            MyLogger::error("Connect error for " + server_.ip + ": " + ec.message());
            return;
        }
        MyLogger::info("Connected to " + server_.ip + ":" + std::to_string(server_.port));

        // Prepare the HTTP POST request to the "/broadcast" endpoint.
        req_.version(11);
        req_.method(http::verb::post);
        req_.target("/broadcast");
        req_.set(http::field::host, server_.ip);
        req_.set(http::field::user_agent, BOOST_BEAST_VERSION_STRING);
        req_.set(http::field::content_type, "application/json");
        req_.body() = message_.dump();
        req_.prepare_payload();

        // Asynchronously send the request.
        http::async_write(
            stream_,
            req_,
            std::bind(
                &Session::on_write,
                shared_from_this(),
                std::placeholders::_1,
                std::placeholders::_2));
    }

    // Called after the request is written.
    void on_write(beast::error_code ec, std::size_t bytes_transferred)
    {
        boost::ignore_unused(bytes_transferred);
        if (ec)
        {
            MyLogger::error("Write error for " + server_.ip + ": " + ec.message());
            return;
        }
        MyLogger::info("HTTP POST request sent to " + server_.ip);

        // Asynchronously read the response.
        http::async_read(
            stream_,
            buffer_,
            res_,
            std::bind(
                &Session::on_read,
                shared_from_this(),
                std::placeholders::_1,
                std::placeholders::_2));
    }

    // Called when the response is received.
    void on_read(beast::error_code ec, std::size_t bytes_transferred)
    {
        boost::ignore_unused(bytes_transferred);
        if (ec)
        {
            MyLogger::error("Read error for " + server_.ip + ": " + ec.message());
            return;
        }

        auto bodyStr = boost::beast::buffers_to_string(res_.body().data());
        MyLogger::info("Received response from " + server_.ip + ": " + bodyStr);

        // Shutdown the connection gracefully.
        beast::error_code shutdown_ec;
        stream_.socket().shutdown(tcp::socket::shutdown_both, shutdown_ec);
        if (shutdown_ec && shutdown_ec != beast::errc::not_connected)
        {
            MyLogger::error("Shutdown error for " + server_.ip + ": " + shutdown_ec.message());
            return;
        }
        MyLogger::info("Connection closed gracefully for " + server_.ip);
    }
};

int main(int argc, char *argv[])
{
    if (argc < 2)
    {
        MyLogger::error("Usage: " + std::string(argv[0]) + " <user_id>");
        return EXIT_FAILURE;
    }

    // Prepare a JSON message.
    nlohmann::json message;
    message["message"] = "Hello, World!";
    message["user_id"] = argv[1];

    try
    {
        // Create an io_context for asynchronous operations.
        asio::io_context ioc;

        // Create and run a session for each server in the global list.
        for (const auto &server : serverList)
        {
            std::make_shared<Session>(ioc, server, message)->run();
        }

        // Run the io_context to process all asynchronous events concurrently.
        ioc.run();
    }
    catch (const std::exception &e)
    {
        MyLogger::error("Exception: " + std::string(e.what()));
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
