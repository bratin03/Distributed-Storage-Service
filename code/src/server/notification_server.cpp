// NotificationServer.cpp
#include <httplib.h>
#include <iostream>
#include <string>
#include <map>
#include <queue>
#include <mutex>
#include <chrono>
#include <thread>
#include <nlohmann/json.hpp>
#include <set>

using json = nlohmann::json;

class NotificationServer {
private:
    httplib::Server server;
    
    // Store notifications for each client
    std::map<std::string, std::queue<json>> clientNotifications;
    
    // Track operations that are in progress
    std::map<std::string, std::set<std::string>> pendingOperations; // operation_id -> set of client_ids
    
    // Mutex for thread safety
    std::mutex mtx;

public:
    NotificationServer(int port) {
        // Set up the routes
        setupRoutes();
        
        // Start the server
        std::cout << "Starting notification server on port " << port << std::endl;
        server.listen("0.0.0.0", port);
    }

private:
    void setupRoutes() {
        // Client long polling endpoint
        server.Get("/poll", [this](const httplib::Request& req, httplib::Response& res) {
            handlePoll(req, res);
        });
        
        // Endpoint for metadata server to send notifications
        server.Post("/notify", [this](const httplib::Request& req, httplib::Response& res) {
            handleNotify(req, res);
        });
        
        // Endpoint for metadata server to check notification status
        server.Get("/status", [this](const httplib::Request& req, httplib::Response& res) {
            handleStatus(req, res);
        });
        
        // Endpoint for metadata server to confirm notifications were processed
        server.Post("/confirm", [this](const httplib::Request& req, httplib::Response& res) {
            handleConfirm(req, res);
        });
    }
    
    void handlePoll(const httplib::Request& req, httplib::Response& res) {
        // Get client ID
        std::string clientId = req.get_param_value("client_id");
        if (clientId.empty()) {
            res.status = 400;
            res.set_content("{\"error\":\"Missing client_id parameter\"}", "application/json");
            return;
        }
        
        // Get timeout (default: 30 seconds)
        int timeout = 30;
        if (req.has_param("timeout")) {
            try {
                timeout = std::stoi(req.get_param_value("timeout"));
                // Ensure timeout is reasonable
                timeout = std::max(1, std::min(timeout, 120));
            } catch (...) {
                // Use default on error
            }
        }
        
        // Check if there are existing notifications
        json notification;
        bool hasNotification = false;
        std::string operationId;
        
        {
            std::lock_guard<std::mutex> lock(mtx);
            auto& queue = clientNotifications[clientId];
            
            if (!queue.empty()) {
                // Get the oldest notification
                notification = queue.front();
                queue.pop();
                hasNotification = true;
                
                // If this notification is part of an operation, update pending operations
                if (notification.contains("operation_id")) {
                    operationId = notification["operation_id"];
                    
                    // Remove this client from the pending operations list
                    auto it = pendingOperations.find(operationId);
                    if (it != pendingOperations.end()) {
                        it->second.erase(clientId);
                        
                        // If no more clients are pending for this operation, remove it
                        if (it->second.empty()) {
                            pendingOperations.erase(it);
                        }
                    }
                }
            }
        }
        
        if (hasNotification) {
            // Send the notification
            res.set_content(notification.dump(), "application/json");
            return;
        }
        
        // No immediate notification, do long polling
        auto startTime = std::chrono::steady_clock::now();
        auto endTime = startTime + std::chrono::seconds(timeout);
        
        while (std::chrono::steady_clock::now() < endTime) {
            // Check for new notifications
            {
                std::lock_guard<std::mutex> lock(mtx);
                auto& queue = clientNotifications[clientId];
                
                if (!queue.empty()) {
                    // Get the notification
                    notification = queue.front();
                    queue.pop();
                    hasNotification = true;
                    
                    // Update pending operations if needed
                    if (notification.contains("operation_id")) {
                        operationId = notification["operation_id"];
                        
                        auto it = pendingOperations.find(operationId);
                        if (it != pendingOperations.end()) {
                            it->second.erase(clientId);
                            
                            if (it->second.empty()) {
                                pendingOperations.erase(it);
                            }
                        }
                    }
                    
                    break;
                }
            }
            
            // Sleep to avoid busy waiting
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        
        if (hasNotification) {
            // Send the notification
            res.set_content(notification.dump(), "application/json");
        } else {
            // Timeout occurred
            res.status = 204; // No content
        }
    }
    
    void handleNotify(const httplib::Request& req, httplib::Response& res) {
        // Parse notification request
        json notification;
        try {
            notification = json::parse(req.body);
        } catch (...) {
            res.status = 400;
            res.set_content("{\"error\":\"Invalid JSON\"}", "application/json");
            return;
        }
        
        // Validate required fields
        if (!notification.contains("target_clients")) {
            res.status = 400;
            res.set_content("{\"error\":\"Missing target_clients field\"}", "application/json");
            return;
        }
        
        // Get operation ID if available
        std::string operationId;
        if (notification.contains("operation_id")) {
            operationId = notification["operation_id"];
        } else {
            // Generate a unique operation ID if not provided
            operationId = "op_" + std::to_string(std::chrono::system_clock::now().time_since_epoch().count());
            notification["operation_id"] = operationId;
        }
        
        // Add timestamp if not provided
        if (!notification.contains("timestamp")) {
            notification["timestamp"] = std::chrono::system_clock::now().time_since_epoch().count();
        }
        
        // Extract target clients
        std::vector<std::string> targetClients;
        try {
            for (const auto& client : notification["target_clients"]) {
                targetClients.push_back(client.get<std::string>());
            }
        } catch (...) {
            res.status = 400;
            res.set_content("{\"error\":\"Invalid target_clients format\"}", "application/json");
            return;
        }
        
        // Queue the notification for each target client
        {
            std::lock_guard<std::mutex> lock(mtx);
            
            // Add this operation to pending operations
            auto& clientSet = pendingOperations[operationId];
            
            for (const auto& clientId : targetClients) {
                clientNotifications[clientId].push(notification);
                clientSet.insert(clientId);
            }
        }
        
        // Send success response
        json response = {
            {"success", true},
            {"operation_id", operationId},
            {"queued_for", targetClients.size()}
        };
        
        res.set_content(response.dump(), "application/json");
    }
    
    void handleStatus(const httplib::Request& req, httplib::Response& res) {
        // Check if an operation ID is provided
        if (!req.has_param("operation_id")) {
            // Return general status
            json status = getGeneralStatus();
            res.set_content(status.dump(), "application/json");
            return;
        }
        
        std::string operationId = req.get_param_value("operation_id");
        
        // Get status for this specific operation
        json status = getOperationStatus(operationId);
        res.set_content(status.dump(), "application/json");
    }
    
    void handleConfirm(const httplib::Request& req, httplib::Response& res) {
        // Parse the confirmation request
        json confirmation;
        try {
            confirmation = json::parse(req.body);
        } catch (...) {
            res.status = 400;
            res.set_content("{\"error\":\"Invalid JSON\"}", "application/json");
            return;
        }
        
        // Check for operation ID
        if (!confirmation.contains("operation_id")) {
            res.status = 400;
            res.set_content("{\"error\":\"Missing operation_id field\"}", "application/json");
            return;
        }
        
        std::string operationId = confirmation["operation_id"];
        bool isComplete = false;
        
        {
            std::lock_guard<std::mutex> lock(mtx);
            // Check if operation is complete (not in pending operations)
            isComplete = pendingOperations.find(operationId) == pendingOperations.end();
        }
        
        // Send response
        json response = {
            {"success", true},
            {"operation_id", operationId},
            {"is_complete", isComplete}
        };
        
        res.set_content(response.dump(), "application/json");
    }
    
    json getGeneralStatus() {
        std::lock_guard<std::mutex> lock(mtx);
        
        json status = {
            {"pending_operations", pendingOperations.size()},
            {"client_count", clientNotifications.size()}
        };
        
        return status;
    }
    
    json getOperationStatus(const std::string& operationId) {
        std::lock_guard<std::mutex> lock(mtx);
        
        json status = {
            {"operation_id", operationId},
            {"is_complete", pendingOperations.find(operationId) == pendingOperations.end()}
        };
        
        // If operation is still pending, include details
        auto it = pendingOperations.find(operationId);
        if (it != pendingOperations.end()) {
            status["pending_clients"] = it->second.size();
            
            // Include full client list if it's not too large
            if (it->second.size() <= 100) {
                json clientList = json::array();
                for (const auto& clientId : it->second) {
                    clientList.push_back(clientId);
                }
                status["pending_client_ids"] = clientList;
            }
        }
        
        return status;
    }
};

int main(int argc, char** argv) {
    // Default port
    int port = 8080;
    
    // Parse command line arguments
    if (argc > 1) {
        try {
            port = std::stoi(argv[1]);
        } catch (...) {
            std::cerr << "Invalid port number, using default: " << port << std::endl;
        }
    }
    
    try {
        // Create and start the notification server
        NotificationServer server(port);
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
