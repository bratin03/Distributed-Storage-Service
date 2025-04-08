#ifndef NOTIFICATION_SERVER_HPP
#define NOTIFICATION_SERVER_HPP

#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <boost/beast/http.hpp>
#include <unordered_map>
#include <vector>
#include <mutex>
#include <memory>
#include <string>

namespace notification
{

    namespace beast = boost::beast;
    namespace http = beast::http;
    namespace asio = boost::asio;
    using tcp = asio::ip::tcp;

    // Forward declaration.
    class Session;
    using SessionPtr = std::shared_ptr<Session>;

    // The NotificationServer class encapsulates the long-polling server.
    class NotificationServer
    {
    public:
        // Construct the server with an io_context, IP address, port, and timeout (in seconds).
        NotificationServer(asio::io_context &ioc, const std::string &ip, unsigned short port, int timeout_seconds);

        // Start the server (begin accepting connections).
        void run();

        // API: Broadcast a notification to all sessions currently waiting for the given user_id.
        void broadcastNotification(const std::string &user_id, const std::string &message);

    private:
        void do_accept();

        tcp::acceptor acceptor_;
        int timeout_seconds_;

        // Subscription map: user id to sessions.
        std::unordered_map<std::string, std::vector<SessionPtr>> subscriptions_;
        std::mutex subs_mutex_;

        // Allow Session to access private members.
        friend class Session;
    };

    // Session class represents a client connection.
    class Session : public std::enable_shared_from_this<Session>
    {
    public:
        // Create a session with a socket and a reference to the NotificationServer.
        Session(tcp::socket socket, NotificationServer &server);

        // Start reading the HTTP request.
        void start();

        // Send a notification response and close the connection.
        void sendNotification(const std::string &message);

    private:
        void do_read();
        void handle_request();
        void sendTimeoutResponse();

        tcp::socket socket_;
        beast::flat_buffer buffer_;
        http::request<http::string_body> req_;
        std::string user_id_;
        asio::steady_timer timer_;
        NotificationServer &server_;
    };

} // namespace notification

#endif // NOTIFICATION_SERVER_HPP
