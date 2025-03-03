// other_server.cpp
#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <fstream>
#include <iostream>
#include <sstream>
#include <jwt.h> // libjwt header

using boost::asio::ip::tcp;
namespace ssl = boost::asio::ssl;

// A helper function to verify the token using the public key.
bool verify_token(const std::string &token_str)
{
    jwt_t *jwt = nullptr;
    if (jwt_decode(&jwt, token_str.c_str(), nullptr, 0))
    {
        return false;
    }
    // Read public key from file (login_keys/public.pem)
    std::ifstream key_file("login_keys/public.pem");
    std::stringstream key_stream;
    key_stream << key_file.rdbuf();
    std::string public_key = key_stream.str();

    // Set algorithm and key for verification
    if (jwt_set_alg(jwt, JWT_ALG_RS256, (unsigned char *)public_key.c_str(), public_key.size()) != 0)
    {
        jwt_free(jwt);
        return false;
    }
    // Verify expiration
    time_t exp = jwt_get_grant_int(jwt, "exp");
    if (time(nullptr) > exp)
    {
        jwt_free(jwt);
        return false;
    }
    jwt_free(jwt);
    return true;
}

int main()
{
    try
    {
        boost::asio::io_context io_context;
        // SSL context for the other server using its SSL certificate and key
        ssl::context ctx(ssl::context::tlsv12_server);
        ctx.use_certificate_chain_file("ssl_keys/server.crt");
        ctx.use_private_key_file("ssl_keys/server.key", ssl::context::pem);

        tcp::acceptor acceptor(io_context, tcp::endpoint(tcp::v4(), 9443));
        std::cout << "Other server listening on port 9443...\n";

        while (true)
        {
            tcp::socket socket(io_context);
            acceptor.accept(socket);
            ssl::stream<tcp::socket> ssl_socket(std::move(socket), ctx);
            ssl_socket.handshake(ssl::stream_base::server);

            boost::asio::streambuf buf;
            boost::asio::read_until(ssl_socket, buf, "\n");
            std::istream is(&buf);
            std::string request;
            std::getline(is, request);
            // For simplicity, assume request JSON contains "token": "<jwt_token>"
            auto pos = request.find("\"token\":");
            std::string token;
            if (pos != std::string::npos)
            {
                auto start = request.find("\"", pos + 8) + 1;
                auto end = request.find("\"", start);
                token = request.substr(start, end - start);
            }

            std::string response;
            if (!token.empty() && verify_token(token))
            {
                std::cout << "Token verified successfully.\n";
                response = "{\"status\": \"OK\"}\n";
            }
            else
            {
                std::cout << "Token verification failed.\n";
                response = "{\"status\": \"Unauthorized\"}\n";
            }
            boost::asio::write(ssl_socket, boost::asio::buffer(response));
            ssl_socket.shutdown();
        }
    }
    catch (std::exception &e)
    {
        std::cerr << "Exception in other server: " << e.what() << "\n";
    }
    return 0;
}
