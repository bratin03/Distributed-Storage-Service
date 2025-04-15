// login_server.cpp
#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <fstream>
#include <iostream>
#include <sstream>
#include <map>
#include <jwt.h> // libjwt header

using boost::asio::ip::tcp;
namespace ssl = boost::asio::ssl;

std::map<std::string, std::string> load_credentials(const std::string &filename)
{
    std::map<std::string, std::string> creds;
    std::ifstream file(filename);
    std::string line;
    while (std::getline(file, line))
    {
        std::istringstream iss(line);
        std::string ip, password;
        if (iss >> ip >> password)
        {
            creds[ip] = password;
        }
    }
    return creds;
}

std::string generate_token(const std::string &client_ip)
{
    // Create a JWT token and sign it with the private key (using RSA)
    jwt_t *jwt = nullptr;
    jwt_new(&jwt);
    jwt_add_grant(jwt, "client_ip", client_ip.c_str());
    jwt_add_grant_int(jwt, "exp", time(nullptr) + 3600); // 1-hour expiry
    jwt_set_alg(jwt, JWT_ALG_RS256, nullptr, 0);         // we'll sign with RSA

    // Read the private key from file (login_keys/private.pem)
    std::ifstream key_file("login_keys/private.pem");
    std::stringstream key_stream;
    key_stream << key_file.rdbuf();
    std::string private_key = key_stream.str();

    // Sign the token with the private key
    jwt_set_alg(jwt, JWT_ALG_RS256, (unsigned char *)private_key.c_str(), private_key.size());
    char *token = jwt_encode_str(jwt);
    std::string token_str(token);
    free(token);
    jwt_free(jwt);
    return token_str;
}

int main()
{
    try
    {
        boost::asio::io_context io_context;
        // SSL context for the login server using SSL keys
        ssl::context ctx(ssl::context::tlsv12_server);
        ctx.use_certificate_chain_file("ssl_keys/server.crt");
        ctx.use_private_key_file("ssl_keys/server.key", ssl::context::pem);

        tcp::acceptor acceptor(io_context, tcp::endpoint(tcp::v4(), 8443));
        std::cout << "Login server listening on port 8443...\n";

        auto credentials = load_credentials("login.txt");

        while (true)
        {
            tcp::socket socket(io_context);
            acceptor.accept(socket);
            // Upgrade plain socket to SSL stream.
            ssl::stream<tcp::socket> ssl_socket(std::move(socket), ctx);
            ssl_socket.handshake(ssl::stream_base::server);

            // Get remote client IP
            std::string client_ip = ssl_socket.lowest_layer().remote_endpoint().address().to_string();

            // Read client's password (assume client sends a single line password)
            boost::asio::streambuf buf;
            boost::asio::read_until(ssl_socket, buf, "\n");
            std::istream is(&buf);
            std::string password;
            std::getline(is, password);

            std::string response;
            // Validate password based on client's IP from login.txt
            if (credentials.count(client_ip) && credentials[client_ip] == password)
            {
                std::string token = generate_token(client_ip);
                // Hardcode next server IP (could be read from config)
                std::string next_server_ip = "127.0.0.1";
                // Build a simple JSON response
                response = "{\"next_server_ip\": \"" + next_server_ip + "\", \"token\": \"" + token + "\"}\n";
            }
            else
            {
                response = "{\"error\": \"Invalid credentials\"}\n";
            }
            boost::asio::write(ssl_socket, boost::asio::buffer(response));
            ssl_socket.shutdown();
        }
    }
    catch (std::exception &e)
    {
        std::cerr << "Exception in login server: " << e.what() << "\n";
    }
    return 0;
}
