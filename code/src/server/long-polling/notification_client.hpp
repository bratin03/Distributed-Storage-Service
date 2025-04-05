#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>
#include <boost/asio.hpp>
#include <nlohmann/json.hpp>
#include <iostream>
#include <string>
#include <queue>
#include <mutex>
#include <thread>
#include <chrono>

namespace beast = boost::beast;       // from <boost/beast.hpp>
namespace http = beast::http;         // from <boost/beast/http.hpp>
namespace asio = boost::asio;         // from <boost/asio.hpp>
using tcp = asio::ip::tcp;
using json = nlohmann::json;

class NotificationClient {
public:
    // Constructor takes the server IP, port, user ID, queue reference, and queue mutex reference.
    NotificationClient(const std::string &ip, int port, const std::string &user_id,
                       std::queue<json> &notification_queue, std::mutex &queue_lock)
        : server_ip(ip), server_port(port), user_id(user_id),
          notification_queue(notification_queue), queue_lock(queue_lock),
          stop_flag(false) {}

    // Start the polling loop in a separate thread.
    void start() {
        polling_thread = std::thread(&NotificationClient::polling_loop, this);
    }

    // Signal the polling loop to stop and join the thread.
    void stop() {
        stop_flag = true;
        if (polling_thread.joinable()) {
            polling_thread.join();
        }
    }

    // Destructor ensures the polling thread is properly stopped.
    ~NotificationClient() {
        stop();
    }

private:
    std::string server_ip;
    int server_port;
    std::string user_id;
    std::queue<json> &notification_queue;
    std::mutex &queue_lock;
    std::thread polling_thread;
    bool stop_flag;

    // The polling loop that continuously connects, retrieves notifications, and pushes them to the queue.
    void polling_loop() {
        while (!stop_flag) {
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

                // Buffer for reading.
                beast::flat_buffer buffer;
                http::response<http::string_body> res;
                http::read(socket, buffer, res);

                // Parse the response body as JSON.
                json notification = json::parse(res.body(), nullptr, false);
                if (notification.is_discarded()) {
                    std::cerr << "Failed to parse JSON notification: " << res.body() << "\n";
                } else {
                    // Lock and push the notification onto the shared queue.
                    std::lock_guard<std::mutex> lock(queue_lock);
                    notification_queue.push(notification);
                }

                // Shutdown the socket gracefully.
                boost::system::error_code ec;
                socket.shutdown(tcp::socket::shutdown_both, ec);
            } catch (std::exception &e) {
                std::cerr << "NotificationClient error: " << e.what() << "\n";
            }
            // Sleep a short duration before polling again.
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }
};
