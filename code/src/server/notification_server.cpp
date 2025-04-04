// notification_server.cpp
#include <httplib.h>
#include <iostream>
#include <map>
#include <mutex>
#include <string>
#include <vector>
#include <chrono>
#include <thread>
#include <queue>
#include <condition_variable>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

struct Notification {
    std::string userID;
    std::string type; // directory_deleted, file_deleted, etc.
    json data;
    std::string metadataServerCallback; // URL to call back to confirm delivery
};

class NotificationServer {
private:
    httplib::Server server;
    std::map<std::string, std::vector<httplib::Response*>> pendingRequests;
    std::queue<Notification> notificationQueue;
    std::mutex mutex;
    std::condition_variable cv;
    const int MAX_POLL_TIMEOUT = 60; // seconds
    
    void sendConfirmation(const Notification& notification) {
        if (notification.metadataServerCallback.empty()) {
            return; // No callback URL provided
        }
        
        try {
            // Extract host and path from callback URL
            std::string url = notification.metadataServerCallback;
            size_t protocolEnd = url.find("://");
            size_t hostStart = (protocolEnd != std::string::npos) ? protocolEnd + 3 : 0;
            size_t pathStart = url.find("/", hostStart);
            
            std::string host;
            std::string path;
            
            if (pathStart != std::string::npos) {
                host = url.substr(hostStart, pathStart - hostStart);
                path = url.substr(pathStart);
            } else {
                host = url.substr(hostStart);
                path = "/";
            }
            
            // Check for port in host
            int port = 80;
            size_t portPos = host.find(":");
            if (portPos != std::string::npos) {
                port = std::stoi(host.substr(portPos + 1));
                host = host.substr(0, portPos);
            }
            
            // Prepare confirmation data
            json confirmationData = {
                {"userID", notification.userID},
                {"type", notification.type},
                {"data", notification.data}
            };
            
            // Send confirmation back to metadata server
            httplib::Client client(host, port);
            auto res = client.Post(path, confirmationData.dump(), "application/json");
            
            if (res && res->status == 200) {
                std::cout << "Notification delivery confirmed to metadata server" << std::endl;
            } else {
                std::cerr << "Failed to confirm notification delivery to metadata server" << std::endl;
            }
        } catch (const std::exception& e) {
            std::cerr << "Error sending confirmation: " << e.what() << std::endl;
        }
    }
    
    void processNotificationsThread() {
        while (true) {
            Notification notification;
            {
                std::unique_lock<std::mutex> lock(mutex);
                cv.wait(lock, [this]{ return !notificationQueue.empty(); });
                notification = notificationQueue.front();
                notificationQueue.pop();
            }
            
            bool delivered = false;
            {
                std::unique_lock<std::mutex> lock(mutex);
                if (pendingRequests.count(notification.userID)) {
                    json responseData = {
                        {"type", notification.type},
                        {"data", notification.data}
                    };
                    
                    std::string responseStr = responseData.dump();
                    for (auto* response : pendingRequests[notification.userID]) {
                        response->set_content(responseStr, "application/json");
                    }
                    
                    // Clear the pending requests for this user
                    pendingRequests.erase(notification.userID);
                    delivered = true;
                }
            }
            
            if (delivered) {
                // Send confirmation back to metadata server
                sendConfirmation(notification);
            } else {
                std::cout << "No connected clients for userID: " << notification.userID << std::endl;
                // Store notification for delayed delivery if needed
                // Or immediately confirm that no clients are available
                sendConfirmation(notification);
            }
        }
    }
    
public:
    NotificationServer(int port) {
        // Start notification processing thread
        std::thread processor(&NotificationServer::processNotificationsThread, this);
        processor.detach();
        
        // Handle long polling requests
        server.Get("/poll", [this](const httplib::Request& req, httplib::Response& res) {
            if (!req.has_param("userID")) {
                res.status = 400;
                res.set_content("Missing userID parameter", "text/plain");
                return;
            }
            
            std::string userID = req.get_param_value("userID");
            auto startTime = std::chrono::steady_clock::now();
            bool timeout = false;
            
            // Register this client for notifications
            {
                std::unique_lock<std::mutex> lock(mutex);
                pendingRequests[userID].push_back(&res);
            }
            
            // Wait until either response is set or timeout occurs
            while (!res.body.size() && !timeout) {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                auto now = std::chrono::steady_clock::now();
                auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - startTime).count();
                
                if (elapsed >= MAX_POLL_TIMEOUT) {
                    timeout = true;
                }
            }
            
            // If timed out, remove this response from pending requests
            if (timeout) {
                std::unique_lock<std::mutex> lock(mutex);
                auto& responses = pendingRequests[userID];
                responses.erase(std::remove(responses.begin(), responses.end(), &res), responses.end());
                if (responses.empty()) {
                    pendingRequests.erase(userID);
                }
                res.status = 204; // No Content
            }
        });
        
        // Handle notification submissions from metadata server
        server.Post("/notify", [this](const httplib::Request& req, httplib::Response& res) {
            try {
                json notification = json::parse(req.body);
                
                if (!notification.contains("userID") || 
                    !notification.contains("type") || 
                    !notification.contains("data")) {
                    res.status = 400;
                    res.set_content("Invalid notification format", "text/plain");
                    return;
                }
                
                // Extract callback URL if provided
                std::string callbackUrl = "";
                if (notification.contains("callback")) {
                    callbackUrl = notification["callback"].get<std::string>();
                }
                
                // Queue notification for processing
                {
                    std::unique_lock<std::mutex> lock(mutex);
                    notificationQueue.push({
                        notification["userID"].get<std::string>(),
                        notification["type"].get<std::string>(),
                        notification["data"],
                        callbackUrl
                    });
                    cv.notify_one();
                }
                
                res.set_content("Notification queued", "text/plain");
            } catch (const std::exception& e) {
                res.status = 400;
                res.set_content(std::string("Error: ") + e.what(), "text/plain");
            }
        });
        
        std::cout << "Starting notification server on port " << port << std::endl;
        server.listen("0.0.0.0", port);
    }
};

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " <port>" << std::endl;
        return 1;
    }
    
    int port = std::stoi(argv[1]);
    NotificationServer server(port);
    return 0;
}
