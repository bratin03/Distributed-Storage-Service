// notification_client.cpp
#include <httplib.h>
#include <iostream>
#include <string>
#include <thread>
#include <atomic>
#include <chrono>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

class NotificationClient {
private:
    std::string serverUrl;
    std::string userID;
    std::atomic<bool> running;
    
    void processNotification(const json& notification) {
        std::string type = notification["type"];
        json data = notification["data"];
        
        std::cout << "Received notification: " << type << std::endl;
        std::cout << "Data: " << data.dump(2) << std::endl;
        
        if (type == "directory_deleted") {
            std::cout << "Directory deleted: " << data["path"] << std::endl;
        } else if (type == "file_modified") {
            std::cout << "File modified: " << data["path"] << std::endl;
        } else if (type == "directory_created") {
            std::cout << "Directory created: " << data["path"] << std::endl;
        } else if (type == "file_created") {
            std::cout << "File created: " << data["path"] << std::endl;
        }
    }
    
    void pollForNotifications() {
        httplib::Client client(serverUrl);
        
        while (running) {
            std::cout << "Starting long poll..." << std::endl;
            
            auto res = client.Get("/poll?userID=" + httplib::detail::encode_url(userID));
            
            if (res && res->status == 200) {
                try {
                    json notification = json::parse(res->body);
                    processNotification(notification);
                } catch (const std::exception& e) {
                    std::cerr << "Error parsing notification: " << e.what() << std::endl;
                }
            } else if (res && res->status == 204) {
                // Long polling timed out, reconnect
                std::cout << "Long polling timed out, reconnecting..." << std::endl;
            } else {
                std::cerr << "Error in long polling: ";
                if (res) {
                    std::cerr << "Status " << res->status << std::endl;
                } else {
                    std::cerr << "Connection failed" << std::endl;
                }
                
                // Wait before reconnecting
                std::this_thread::sleep_for(std::chrono::seconds(5));
            }
        }
    }
    
public:
    NotificationClient(const std::string& serverUrl, const std::string& userID) 
        : serverUrl(serverUrl), userID(userID), running(true) {
        
        std::thread pollingThread(&NotificationClient::pollForNotifications, this);
        pollingThread.detach();
    }
    
    void stop() {
        running = false;
    }
};

int main(int argc, char* argv[]) {
    if (argc != 3) {
        std::cerr << "Usage: " << argv[0] << " <notification_server_url> <userID>" << std::endl;
        return 1;
    }
    
    std::string serverUrl = argv[1];
    std::string userID = argv[2];
    
    NotificationClient client(serverUrl, userID);
    
    std::cout << "Notification client started for user " << userID << std::endl;
    std::cout << "Press Enter to exit..." << std::endl;
    
    std::cin.get();
    client.stop();
    
    return 0;
}
