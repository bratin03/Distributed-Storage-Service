// deletion_client.cpp
#include <httplib.h>
#include <iostream>
#include <string>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

class DeletionClient {
private:
    std::string metadataServerUrl;
    std::string userID;
    
public:
    DeletionClient(const std::string& metadataServerUrl, const std::string& userID) 
        : metadataServerUrl(metadataServerUrl), userID(userID) {}
    
    bool deleteDirectory(const std::string& dirPath) {
        httplib::Client client(metadataServerUrl);
        
        std::string path = "/dir?path=" + httplib::detail::encode_url(dirPath) + 
                          "&userID=" + httplib::detail::encode_url(userID);
        
        auto res = client.Delete(path);
        
        if (!res) {
            std::cerr << "Connection error" << std::endl;
            return false;
        }
        
        if (res->status == 200) {
            try {
                json responseData = json::parse(res->body);
                
                if (responseData["status"] == "deleted") {
                    std::cout << "Directory deleted successfully: " << responseData["path"] << std::endl;
                    return true;
                } else if (responseData["status"] == "not_empty") {
                    std::cout << "Directory is not empty. Contains:" << std::endl;
                    
                    // Print files
                    if (!responseData["files"].empty()) {
                        std::cout << "Files:" << std::endl;
                        for (auto& [fileName, servers] : responseData["files"].items()) {
                            std::cout << "  " << fileName << " on servers: ";
                            for (const auto& server : servers) {
                                std::cout << server << " ";
                            }
                            std::cout << std::endl;
                        }
                    }
                    
                    // Print subdirectories
                    if (!responseData["subdirectories"].empty()) {
                        std::cout << "Subdirectories:" << std::endl;
                        for (auto& [subdir, servers] : responseData["subdirectories"].items()) {
                            std::cout << "  " << subdir << " on servers: ";
                            for (const auto& server : servers) {
                                std::cout << server << " ";
                            }
                            std::cout << std::endl;
                        }
                    }
                    
                    return false;
                } else {
                    std::cerr << "Unknown status: " << responseData["status"] << std::endl;
                    return false;
                }
            } catch (const std::exception& e) {
                std::cerr << "Error parsing response: " << e.what() << std::endl;
                return false;
            }
        } else {
            std::cerr << "Error: " << res->status << " " << res->body << std::endl;
            return false;
        }
    }
};

int main(int argc, char* argv[]) {
    if (argc != 4) {
        std::cerr << "Usage: " << argv[0] << " <metadata_server_url> <userID> <directory_path>" << std::endl;
        return 1;
    }
    
    std::string serverUrl = argv[1];
    std::string userID = argv[2];
    std::string dirPath = argv[3];
    
    DeletionClient client(serverUrl, userID);
    
    std::cout << "Attempting to delete directory: " << dirPath << std::endl;
    bool result = client.deleteDirectory(dirPath);
    
    if (!result) {
        std::cout << "Directory was not deleted." << std::endl;
    }
    
    return 0;
}
