#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>
#include <boost/asio.hpp>
#include <nlohmann/json.hpp>
#include <fstream>
#include <iostream>
#include <string>
#include <thread>

namespace beast = boost::beast;       // from <boost/beast.hpp>
namespace http = beast::http;         // from <boost/beast/http.hpp>
namespace asio = boost::asio;         // from <boost/asio.hpp>
using tcp = asio::ip::tcp;
using json = nlohmann::json;

void long_poll(const std::string &server_ip, int server_port, const std::string &user_id) {
    try {
        asio::io_context ioc;
        tcp::resolver resolver(ioc);
        auto const results = resolver.resolve(server_ip, std::to_string(server_port));
        tcp::socket socket(ioc);
        asio::connect(socket, results.begin(), results.end());

        // Form the HTTP GET request for long polling.
        std::string target = "/subscribe?userid=" + user_id;
        http::request<http::empty_body> req{http::verb::get, target, 11};
        req.set(http::field::host, server_ip);
        req.set(http::field::user_agent, BOOST_BEAST_VERSION_STRING);

        // Send the HTTP request.
        http::write(socket, req);

        // This buffer is used for reading and must be persisted.
        beast::flat_buffer buffer;
        http::response<http::string_body> res;
        http::read(socket, buffer, res);

        json notification = json::parse(res.body(), nullptr, false);
        // Print the response body.
        std::cout << "Response: " << notification << std::endl;

        // Close the connection.
        boost::system::error_code ec;
        socket.shutdown(tcp::socket::shutdown_both, ec);
    } catch (std::exception &e) {
        std::cerr << "Client error: " << e.what() << "\n";
    }
}

int main(int argc, char* argv[]) {
    try {
        // Load configuration from file.
        std::ifstream ifs("config.json");
        if (!ifs) {
            std::cerr << "Failed to open config.json\n";
            return 1;
        }
        json config;
        ifs >> config;

        std::string server_ip = config.value("server_ip", "127.0.0.1");
        int server_port = config.value("server_port", 8080);

        // Check if user ID was provided as a command-line argument.
        std::string user_id;
        if (argc > 1) {
            user_id = argv[1];
        } else {
            user_id = config.value("user_id", "default");
        }

        // Continuously long poll.
        while (true) {
            long_poll(server_ip, server_port, user_id);
            // Reconnect after receiving response.
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    } catch (std::exception &e) {
        std::cerr << "Error: " << e.what() << "\n";
    }
    return 0;
}