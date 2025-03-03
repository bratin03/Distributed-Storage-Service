#include <iostream>
#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>

using boost::asio::ip::tcp;
namespace ssl = boost::asio::ssl;

int main()
{
    try
    {
        boost::asio::io_context io;
        ssl::context ctx(ssl::context::sslv23); // Use a flexible SSL method.

        // Set verification mode and load the system's CA certificates.
        ctx.set_verify_mode(ssl::verify_peer);
        ctx.set_default_verify_paths();

        // Create the SSL stream (a TCP socket layered with SSL)
        ssl::stream<tcp::socket> ssl_socket(io, ctx);

        // Resolve the host.
        tcp::resolver resolver(io);
        auto endpoints = resolver.resolve("www.example.com", "443");

        // Connect the underlying TCP socket.
        boost::asio::connect(ssl_socket.lowest_layer(), endpoints);

        // Perform the SSL handshake.
        ssl_socket.handshake(ssl::stream_base::client);

        // Formulate an HTTP GET request.
        std::string request = "GET / HTTP/1.1\r\nHost: www.example.com\r\nConnection: close\r\n\r\n";
        boost::asio::write(ssl_socket, boost::asio::buffer(request));

        // Read and output the response.
        for (;;)
        {
            char buf[1024];
            boost::system::error_code ec;
            std::size_t len = ssl_socket.read_some(boost::asio::buffer(buf), ec);
            if (ec == boost::asio::error::eof)
                break; // Connection closed cleanly.
            if (ec)
                throw boost::system::system_error(ec);
            std::cout.write(buf, len);
        }
    }
    catch (std::exception &e)
    {
        std::cerr << "Exception: " << e.what() << "\n";
    }
    return 0;
}
