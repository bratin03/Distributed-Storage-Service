#include "http_listener.hpp"
#include <boost/beast.hpp>
#include <boost/asio.hpp>
#include <iostream>
#include <cstdlib>
#include <chrono>

// For potential JSON parsing, you might include nlohmann/json.hpp if needed.
// #include <nlohmann/json.hpp> 
// using json = nlohmann::json;

namespace beast = boost::beast;
namespace http  = beast::http;
namespace asio  = boost::asio;
using tcp       = asio::ip::tcp;

namespace http_listener {

namespace {
//------------------------------------------------------------------------------

/*
   Session class handles a single HTTP connection.
   In this example, it looks for a POST to "/broadcast".
*/
class Session : public std::enable_shared_from_this<Session> {
public:
    Session(tcp::socket socket, RequestHandler handler)
        : socket_(std::move(socket)), handler_(handler)
    {
    }

    void start() {
        doRead();
    }

private:
    tcp::socket socket_;
    beast::flat_buffer buffer_;
    http::request<http::string_body> req_;
    RequestHandler handler_;

    void doRead() {
        auto self = shared_from_this();
        http::async_read(socket_, buffer_, req_,
            [self](beast::error_code ec, std::size_t /*bytes_transferred*/) {
                if (!ec)
                    self->handleRequest();
                else
                    std::cerr << "Read error: " << ec.message() << "\n";
            });
    }

    void handleRequest() {
        // Only process POST requests to "/broadcast"
        if (req_.method() == http::verb::post && req_.target() == "/broadcast") {
            // Call the user-provided request handler with the request body.
            handler_(req_.body());
        }
        
        // Prepare an HTTP response.
        http::response<http::string_body> res{http::status::ok, req_.version()};
        res.set(http::field::server, "HttpListener");
        res.set(http::field::content_type, "text/plain");
        res.body() = "Notification processed";
        res.prepare_payload();

        auto self = shared_from_this();
        http::async_write(socket_, res,
            [self](beast::error_code ec, std::size_t) {
                if(ec)
                    std::cerr << "Write error: " << ec.message() << "\n";
                // Close the socket after sending the response.
                beast::error_code ec_shutdown;
                self->socket_.shutdown(tcp::socket::shutdown_send, ec_shutdown);
            });
    }
};

//------------------------------------------------------------------------------

// The implementation class for HttpListener.
class HttpListener::Impl {
public:
    Impl(asio::io_context &ioc,
         const std::string &ip,
         unsigned short port,
         RequestHandler handler)
        : ioc_(ioc),
          acceptor_(ioc),
          handler_(handler)
    {
        // Create an endpoint from the given IP address and port.
        tcp::endpoint endpoint{asio::ip::make_address(ip), port};

        beast::error_code ec;
        // Open the acceptor.
        acceptor_.open(endpoint.protocol(), ec);
        if (ec) {
            std::cerr << "Acceptor open error: " << ec.message() << "\n";
            return;
        }

        // Allow address reuse.
        acceptor_.set_option(asio::socket_base::reuse_address(true), ec);
        if (ec) {
            std::cerr << "Set option error: " << ec.message() << "\n";
            return;
        }

        // Bind to the server address.
        acceptor_.bind(endpoint, ec);
        if (ec) {
            std::cerr << "Bind error: " << ec.message() << "\n";
            return;
        }

        // Start listening.
        acceptor_.listen(asio::socket_base::max_listen_connections, ec);
        if (ec) {
            std::cerr << "Listen error: " << ec.message() << "\n";
            return;
        }
    }

    // Start accepting new connections.
    void run() {
        doAccept();
    }

private:
    asio::io_context &ioc_;
    tcp::acceptor acceptor_;
    RequestHandler handler_;

    void doAccept() {
        acceptor_.async_accept(
            [this](beast::error_code ec, tcp::socket socket) {
                if (!ec) {
                    // Create a session and start it.
                    std::make_shared<Session>(std::move(socket), handler_)->start();
                } else {
                    std::cerr << "Accept error: " << ec.message() << "\n";
                }
                // Continue accepting additional connections.
                doAccept();
            }
        );
    }
};

} // unnamed namespace

//------------------------------------------------------------------------------
// Public HttpListener methods.

HttpListener::HttpListener(asio::io_context &ioc,
                           const std::string &ip,
                           unsigned short port,
                           RequestHandler handler)
    : impl_(std::make_unique<Impl>(ioc, ip, port, handler))
{
}

void HttpListener::run() {
    if (impl_) {
        impl_->run();
    }
}

HttpListener::~HttpListener() = default;

} // namespace http_listener
