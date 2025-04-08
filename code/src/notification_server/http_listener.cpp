#include "http_listener.hpp"
#include "logger/Mylogger.h" // Include your custom logger.
#include <boost/beast.hpp>
#include <boost/asio.hpp>
#include <iostream>
#include <cstdlib>
#include <chrono>

// For potential JSON parsing, you might include nlohmann/json.hpp if needed.
// #include <nlohmann/json.hpp>
// using json = nlohmann::json;

namespace beast = boost::beast;
namespace http = beast::http;
namespace asio = boost::asio;
using tcp = asio::ip::tcp;

namespace http_listener
{
    //------------------------------------------------------------------------------

    /*
       Session class handles a single HTTP connection.
       In this example, it looks for a POST to "/broadcast".
    */
    class Session : public std::enable_shared_from_this<Session>
    {
    public:
        Session(tcp::socket socket, RequestHandler handler)
            : socket_(std::move(socket)), handler_(handler)
        {
            MyLogger::info("Session created.");
        }

        void start()
        {
            MyLogger::info("Session started.");
            doRead();
        }

    private:
        tcp::socket socket_;
        beast::flat_buffer buffer_;
        http::request<http::string_body> req_;
        RequestHandler handler_;

        void doRead()
        {
            auto self = shared_from_this();
            http::async_read(socket_, buffer_, req_,
                             [self](beast::error_code ec, std::size_t /*bytes_transferred*/)
                             {
                                 if (!ec)
                                 {
                                     MyLogger::info("Request read successfully.");
                                     self->handleRequest();
                                 }
                                 else
                                 {
                                     MyLogger::error("Read error: " + ec.message());
                                 }
                             });
        }

        void handleRequest()
        {
            // Only process POST requests to "/broadcast"
            if (req_.method() == http::verb::post && req_.target() == "/broadcast")
            {
                MyLogger::info("Processing /broadcast request.");
                // Call the user-provided request handler with the request body.
                handler_(req_.body());
            }

            // Prepare an HTTP response and allocate it on the heap.
            auto res = std::make_shared<http::response<http::string_body>>(http::response<http::string_body>{http::status::ok, req_.version()});
            res->set(http::field::server, "HttpListener");
            res->set(http::field::content_type, "text/plain");
            res->body() = "Notification processed";
            res->prepare_payload();

            auto self = shared_from_this();
            http::async_write(socket_, *res,
                              [self, res](beast::error_code ec, std::size_t)
                              {
                                  if (ec)
                                  {
                                      MyLogger::error("Write error: " + ec.message());
                                  }
                                  MyLogger::info("Response sent, shutting down socket.");
                                  beast::error_code ec_shutdown;
                                  self->socket_.shutdown(tcp::socket::shutdown_send, ec_shutdown);
                              });
        }
    };

    //------------------------------------------------------------------------------

    // The implementation class for HttpListener.
    class HttpListener::Impl
    {
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
            if (ec)
            {
                MyLogger::error("Acceptor open error: " + ec.message());
                return;
            }

            // Allow address reuse.
            acceptor_.set_option(asio::socket_base::reuse_address(true), ec);
            if (ec)
            {
                MyLogger::error("Set option error: " + ec.message());
                return;
            }

            // Bind to the server address.
            acceptor_.bind(endpoint, ec);
            if (ec)
            {
                MyLogger::error("Bind error: " + ec.message());
                return;
            }

            // Start listening.
            acceptor_.listen(asio::socket_base::max_listen_connections, ec);
            if (ec)
            {
                MyLogger::error("Listen error: " + ec.message());
                return;
            }
            MyLogger::info("HttpListener initialized successfully on " + ip + ":" + std::to_string(port));
        }

        // Start accepting new connections.
        void run()
        {
            MyLogger::info("HttpListener running and ready to accept connections.");
            doAccept();
        }

    private:
        asio::io_context &ioc_;
        tcp::acceptor acceptor_;
        RequestHandler handler_;

        void doAccept()
        {
            acceptor_.async_accept(
                [this](beast::error_code ec, tcp::socket socket)
                {
                    if (!ec)
                    {
                        MyLogger::info("Accepted new connection.");
                        // Create a session and start it.
                        std::make_shared<Session>(std::move(socket), handler_)->start();
                    }
                    else
                    {
                        MyLogger::error("Accept error: " + ec.message());
                    }
                    // Continue accepting additional connections.
                    doAccept();
                });
        }
    };

    //------------------------------------------------------------------------------
    // Public HttpListener methods.

    HttpListener::HttpListener(asio::io_context &ioc,
                               const std::string &ip,
                               unsigned short port,
                               RequestHandler handler)
        : impl_(std::make_unique<Impl>(ioc, ip, port, handler))
    {
        MyLogger::info("HttpListener created.");
    }

    void HttpListener::run()
    {
        if (impl_)
        {
            impl_->run();
        }
    }

    HttpListener::~HttpListener() = default;

} // namespace http_listener
