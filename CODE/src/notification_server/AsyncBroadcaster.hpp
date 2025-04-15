#ifndef ASYNC_BROADCASTER_HPP
#define ASYNC_BROADCASTER_HPP

#include "logger/Mylogger.h"
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>
#include <boost/asio.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>
#include <memory>
#include <iostream>
#include <functional>
#include <sstream>
#include <thread>
#include <curl/curl.h>

namespace async_broadcast
{

    // Simple structure holding the server IP and port.
    struct Server
    {
        std::string ip;
        unsigned short port;
    };

    /// Class for broadcasting a JSON message concurrently to multiple servers.
    class AsyncBroadcaster
    {
    public:
        /// Constructor takes a list of servers.
        explicit AsyncBroadcaster(const std::vector<Server> &servers)
            : servers_(servers)
        {
        }

        // Function that performs the HTTP POST request to /broadcast for a given server.
        void sendBroadcastRequest(const Server &server, const nlohmann::json &message)
        {
            // Construct the URL as http://<ip>:<port>/broadcast
            std::ostringstream urlStream;
            urlStream << "http://" << server.ip << ":" << server.port << "/broadcast";
            std::string url = urlStream.str();

            // Initialize libcurl
            CURL *curl = curl_easy_init();
            if (curl)
            {
                // Set the URL for the HTTP request.
                curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
                // Specify that this is a POST request.
                curl_easy_setopt(curl, CURLOPT_POST, 1L);

                // Convert the JSON message to a string.
                std::string payload = message.dump();
                curl_easy_setopt(curl, CURLOPT_POSTFIELDS, payload.c_str());

                // Set the HTTP header for JSON content.
                struct curl_slist *headers = nullptr;
                headers = curl_slist_append(headers, "Content-Type: application/json");
                curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

                // Perform the request.
                CURLcode res = curl_easy_perform(curl);
                if (res != CURLE_OK)
                {
                    std::cerr << "Broadcast request to " << url << " failed: "
                              << curl_easy_strerror(res) << std::endl;
                }
                else
                {
                    std::cout << "Broadcast request to " << url << " completed successfully." << std::endl;
                }

                // Cleanup.
                curl_slist_free_all(headers);
                curl_easy_cleanup(curl);
            }
            else
            {
                std::cerr << "Failed to initialize CURL for server " << server.ip << std::endl;
            }
        }

        // The broadcast function accepts a JSON message. It spawns a detached thread per server
        // so that the requests are sent concurrently and do not block each other.
        void broadcast(const nlohmann::json &message)
        {
            for (const auto &server : servers_)
            {
                // Launch a new detached thread calling the member function sendBroadcastRequest.
                std::thread(&AsyncBroadcaster::sendBroadcastRequest, this, server, message).detach();
            }
        }

    private:
        std::vector<Server> servers_;
    };

} // End namespace async_broadcast

#endif // ASYNC_BROADCASTER_HPP

