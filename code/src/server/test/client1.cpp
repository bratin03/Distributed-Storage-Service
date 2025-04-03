// client1.cpp
#include <iostream>
#include "httplib.h"
#include "json.hpp"

using json = nlohmann::json;

int main() {
    // Connect to the notification server on port 9090.
    httplib::Client notifClient("127.0.0.1", 9090);
    
    std::cout << "Client1: Waiting for notification from the notification server..." << std::endl;
    
    // Send GET request to /longpoll (this call blocks until a notification is available or times out).
    auto res = notifClient.Get("/longpoll");
    
    if (res && res->status == 200) {
        try {
            auto notification = json::parse(res->body);
            std::cout << "Client1: Received notification: " << notification.dump(4) << std::endl;
        } catch (const std::exception &e) {
            std::cerr << "Client1: Failed to parse JSON notification: " << e.what() << std::endl;
        }
    } else {
        std::cerr << "Client1: Failed to receive a notification. Response status: " 
                  << (res ? std::to_string(res->status) : "No response") << std::endl;
    }
    
    return 0;
}
