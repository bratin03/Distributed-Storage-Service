// client2.cpp
#include <iostream>
#include "httplib.h"
#include "json.hpp"

using json = nlohmann::json;

int main() {
    // Connect to the metadata server on port 8081.
    httplib::Client metaClient("127.0.0.1", 8081);
    
    // Directory to be deleted. Adjust these values as needed.
    std::string dirPath = "/exampleDir";
    std::string parentDir = "/";  // Example parent directory
    
    // Construct the URL with query parameters.
    std::string url = "/deleteDirectory?path=" + dirPath + "&parent=" + parentDir;
    
    std::cout << "Client2: Sending DELETE request to delete directory " << dirPath << std::endl;
    
    // Send DELETE request.
    auto res = metaClient.Delete(url.c_str());
    
    if (res && res->status == 200) {
        std::cout << "Client2: Deletion response: " << res->body << std::endl;
    } else {
        std::cerr << "Client2: Failed to delete directory. Response: " 
                  << (res ? res->body : "No response") << std::endl;
    }
    
    return 0;
}
