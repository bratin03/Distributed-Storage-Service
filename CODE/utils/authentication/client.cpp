// client.cpp
#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <iostream>
#include <sstream>

// #define CORRUPT_TOKEN

using boost::asio::ip::tcp;
namespace ssl = boost::asio::ssl;

std::string send_receive(ssl::stream<tcp::socket> &stream, const std::string &message)
{
    boost::asio::write(stream, boost::asio::buffer(message));
    boost::asio::streambuf buf;
    boost::asio::read_until(stream, buf, "\n");
    std::istream is(&buf);
    std::string response;
    std::getline(is, response);
    return response;
}

int main()
{
    try
    {
        boost::asio::io_context io_context;

        // Set up SSL context for client (for simplicity, we disable certificate verification here)
        ssl::context ctx(ssl::context::tlsv12_client);
        ctx.set_verify_mode(ssl::verify_none);

        // 1. Connect to the login server
        tcp::resolver resolver(io_context);
        auto endpoints = resolver.resolve("127.0.0.1", "8443");
        ssl::stream<tcp::socket> login_stream(io_context, ctx);
        boost::asio::connect(login_stream.lowest_layer(), endpoints);
        login_stream.handshake(ssl::stream_base::client);

        // For demonstration, client sends its password (matching login.txt)
        std::string password = "password123\n";
        std::string login_response = send_receive(login_stream, password);
        std::cout << "Login response: " << login_response << "\n";
        login_stream.shutdown();

        // For simplicity, parse the JSON manually (in production, use a JSON library)
        std::string next_server_ip, token;
        auto pos1 = login_response.find("\"next_server_ip\":");
        if (pos1 != std::string::npos)
        {
            auto start = login_response.find("\"", pos1 + 18) + 1;
            auto end = login_response.find("\"", start);
            next_server_ip = login_response.substr(start, end - start);
        }
        pos1 = login_response.find("\"token\":");
        if (pos1 != std::string::npos)
        {
            auto start = login_response.find("\"", pos1 + 8) + 1;
            auto end = login_response.find("\"", start);
            token = login_response.substr(start, end - start);
        }

#ifdef CORRUPT_TOKEN
        // Corrupt the token for testing

        token = "corrupt.token.here";
#endif

        // 2. Connect to the other server using the received next_server_ip and a known port (say, 9443)
        endpoints = resolver.resolve(next_server_ip, "9443");
        ssl::stream<tcp::socket> other_stream(io_context, ctx);
        boost::asio::connect(other_stream.lowest_layer(), endpoints);
        other_stream.handshake(ssl::stream_base::client);

        // Build a JSON message with the client's name, its ip (for demonstration, we use local ip), and token
        std::string client_name = "Client1";
        std::string client_ip = "127.0.0.1";
        std::ostringstream oss;

        oss << "{\"name\": \"" << client_name << "\", \"ip\": \"" << client_ip << "\", \"token\": \"" << token << "\"}\n";
        std::string other_response = send_receive(other_stream, oss.str());
        std::cout << "Other server response: " << other_response << "\n";
        other_stream.shutdown();
    }
    catch (std::exception &e)
    {
        std::cerr << "Exception in client: " << e.what() << "\n";
    }
    return 0;
}
