/*
    CS60002 - Distributed Systems
    Term Project - Spring 2025

    * Author 1: Bratin Mondal (21CS10016)
    * Author 2: Soukhin Nayek (21CS10062)
    * Author 3: Swarnabh Mandal (21CS10068)

    * Department of Computer Science and Engineering
    * Indian Institute of Technology, Kharagpur
*/

#ifndef HTTP_LISTENER_HPP
#define HTTP_LISTENER_HPP

#include <boost/asio.hpp>
#include <functional>
#include <memory>
#include <string>

namespace http_listener {

// Type alias for the callback that handles a valid HTTP request body.
using RequestHandler = std::function<void(const std::string &requestBody)>;

// HttpListener provides an asynchronous HTTP server using Boost.Asio & Boost.Beast.
class HttpListener {
   public:
    // Constructor: pass the io_context, IP, port, and a request handler callback.
    HttpListener(boost::asio::io_context &ioc, const std::string &ip, unsigned short port,
                 RequestHandler handler);

    // Start listening for incoming connections.
    void run();

    ~HttpListener();

   private:
    // Private implementation details are hidden using the pImpl pattern.
    class Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace http_listener

#endif  // HTTP_LISTENER_HPP
