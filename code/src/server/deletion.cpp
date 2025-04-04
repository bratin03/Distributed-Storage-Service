// metadata_server.cpp
#include <httplib.h>
#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <mutex>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

// Block server response structure
struct Response {
    bool success;
    std::string value;
    std::string err;
};

class MetadataServer {
private:
    httplib::Server server;
    std::vector<std::string> blockServers;
    std::string notificationServerUrl;
    std::string selfUrl; // URL of this metadata server for callback
    httplib::Client notificationClient;
    
    std::mutex pendingMutex;
    std::map<std::string, json> pendingFileDeletions;
    std::map<std::string, json> pendingDirectoryDeletions;
    
    json getDirectoryMetadata(const std::string& dirPath) {
        Response resp = get(blockServers, dirPath);
        if (!resp.success) {
            return nullptr;
        }
        
        try {
            return json::parse(resp.value);
        } catch (const std::exception& e) {
            std::cerr << "Error parsing metadata: " << e.what() << std::endl;
            return nullptr;
        }
    }
    
    bool updateDirectoryMetadata(const std::string& dirPath, const json& metadata) {
        Response resp = set(blockServers, dirPath, metadata.dump());
        return resp.success;
    }
    
    bool deleteDirectoryMetadata(const std::string& dirPath) {
        Response resp = del(blockServers, dirPath);
        return resp.success;
    }
    
    bool sendNotification(const std::string& userID, const std::string& type, 
                          const json& data, bool waitForConfirmation = true) {
        json notification = {
            {"userID", userID},
            {"type", type},
            {"data", data}
        };
        
        if (waitForConfirmation) {
            notification["callback"] = selfUrl + "/notify-confirmation";
        }
        
        auto res = notificationClient.Post("/notify", notification.dump(), "application/json");
        return res && res->status == 200;
    }
    
    bool updateParentDirectory(const std::string& dirPath, const std::string& dirName) {
        // Extract parent path
        size_t lastSlash = dirPath.find_last_of('/');
        if (lastSlash == std::string::npos) {
            return false;
        }
        
        std::string parentPath = dirPath.substr(0, lastSlash);
        if (parentPath.empty()) {
            parentPath = "/";
        }
        
        // Get parent metadata
        json parentMetadata = getDirectoryMetadata(parentPath);
        if (parentMetadata.is_null()) {
            return false;
        }
        
        // Remove subdirectory from parent
        if (parentMetadata.contains("subdirectories") && 
            parentMetadata["subdirectories"].contains(dirName)) {
            parentMetadata["subdirectories"].erase(dirName);
            
            // Update parent metadata
            return updateDirectoryMetadata(parentPath, parentMetadata);
        }
        
        return false;
    }
    
    // Complete the directory deletion process
    bool completeDirectoryDeletion(const std::string& dirPath) {
        std::unique_lock<std::mutex> lock(pendingMutex);
        
        if (!pendingDirectoryDeletions.count(dirPath)) {
            return false;
        }
        
        json deletionInfo = pendingDirectoryDeletions[dirPath];
        std::string dirName = deletionInfo["dirName"];
        
        // Delete directory metadata
        if (!deleteDirectoryMetadata(dirPath)) {
            std::cerr << "Failed to delete directory metadata: " << dirPath << std::endl;
            return false;
        }
        
        // Update parent directory
        if (!updateParentDirectory(dirPath, dirName)) {
            std::cerr << "Failed to update parent directory for: " << dirPath << std::endl;
            return false;
        }
        
        // Remove from pending deletions
        pendingDirectoryDeletions.erase(dirPath);
        return true;
    }
    
    // Complete file deletion after notification confirmation
    bool completeFileDeletion(const std::string& filePath) {
        std::unique_lock<std::mutex> lock(pendingMutex);
        
        if (!pendingFileDeletions.count(filePath)) {
            return false;
        }
        
        json deletionInfo = pendingFileDeletions[filePath];
        std::string userID = deletionInfo["userID"];
        std::string parentPath = deletionInfo["parentPath"];
        std::string fileName = deletionInfo["fileName"];
        json fileMetadata = deletionInfo["metadata"];
        
        // Get parent directory metadata
        Response parentResp = get(blockServers, parentPath);
        if (!parentResp.success) {
            std::cerr << "Parent directory not found: " << parentPath << std::endl;
            return false;
        }
        
        json parentMetadata;
        try {
            parentMetadata = json::parse(parentResp.value);
        } catch (const std::exception& e) {
            std::cerr << "Error parsing parent metadata: " << e.what() << std::endl;
            return false;
        }
        
        // Delete file chunks from block servers
        if (fileMetadata.contains("chunks")) {
            for (const auto& chunk : fileMetadata["chunks"]) {
                std::string chunkKey = chunk.get<std::string>();
                
                // Delete chunk data
                Response delChunkResp = del(blockServers, chunkKey);
                if (!delChunkResp.success) {
                    std::cerr << "Failed to delete chunk: " << chunkKey << std::endl;
                    // Continue with other chunks even if one fails
                }
                
                // Delete chunk metadata
                std::string chunkMetadataKey = chunkKey + ".metadata";
                Response delChunkMetaResp = del(blockServers, chunkMetadataKey);
                if (!delChunkMetaResp.success) {
                    std::cerr << "Failed to delete chunk metadata: " << chunkMetadataKey << std::endl;
                    // Continue with other chunks even if one fails
                }
            }
        }
        
        // Update parent directory metadata to remove the file
        if (parentMetadata.contains("files")) {
            if (parentMetadata["files"].contains(fileName)) {
                parentMetadata["files"].erase(fileName);
                
                // Update parent metadata in block server
                Response updateResp = set(blockServers, parentPath, parentMetadata.dump());
                if (!updateResp.success) {
                    std::cerr << "Failed to update parent directory metadata" << std::endl;
                    return false;
                }
            } else {
                std::cerr << "File not found in parent directory: " << fileName << std::endl;
                return false;
            }
        } else {
            std::cerr << "Parent directory has no files entry" << std::endl;
            return false;
        }
        
        // Delete file metadata from block server
        Response delMetaResp = del(blockServers, filePath);
        if (!delMetaResp.success) {
            std::cerr << "Failed to delete file metadata" << std::endl;
            return false;
        }
        
        // Remove from pending deletions
        pendingFileDeletions.erase(filePath);
        
        return true;
    }
    
public:
    MetadataServer(int port, const std::vector<std::string>& blockServers, 
                  const std::string& notificationServerUrl, const std::string& selfUrl) 
        : blockServers(blockServers), 
          notificationServerUrl(notificationServerUrl),
          selfUrl(selfUrl),
          notificationClient(notificationServerUrl) {
        
        // Handle directory deletion request
        server.Delete("/dir", [this](const httplib::Request& req, httplib::Response& res) {
            if (!req.has_param("path") || !req.has_param("userID")) {
                res.status = 400;
                res.set_content("Missing path or userID parameter", "text/plain");
                return;
            }
            
            std::string dirPath = req.get_param_value("path");
            std::string userID = req.get_param_value("userID");
            
            // Get directory metadata
            json metadata = getDirectoryMetadata(dirPath);
            if (metadata.is_null()) {
                res.status = 404;
                res.set_content("Directory not found", "text/plain");
                return;
            }
            
            // Check owner
            if (metadata["owner"] != userID) {
                res.status = 403;
                res.set_content("Permission denied", "text/plain");
                return;
            }
            
            // Extract directory name for parent update
            size_t lastSlash = dirPath.find_last_of('/');
            std::string dirName = (lastSlash != std::string::npos) ? 
                                 dirPath.substr(lastSlash + 1) : dirPath;
            
            // Check if directory has files or subdirectories
            bool hasFiles = !metadata["files"].empty();
            bool hasSubdirs = !metadata["subdirectories"].empty();
            
            if (hasFiles || hasSubdirs) {
                // Return endpoints to client for non-empty directories
                json responseData = {
                    {"status", "not_empty"},
                    {"files", metadata["files"]},
                    {"subdirectories", metadata["subdirectories"]}
                };
                
                res.set_content(responseData.dump(), "application/json");
            } else {
                // Directory is empty, proceed with deletion
                
                // Store pending directory deletion info
                {
                    std::unique_lock<std::mutex> lock(pendingMutex);
                    pendingDirectoryDeletions[dirPath] = {
                        {"userID", userID},
                        {"dirPath", dirPath},
                        {"dirName", dirName}
                    };
                }
                
                // Send notification
                json notificationData = {
                    {"path", dirPath},
                    {"action", "deleted"}
                };
                
                if (sendNotification(userID, "directory_deleted", notificationData, true)) {
                    json responseData = {
                        {"status", "deletion_initiated"},
                        {"path", dirPath}
                    };
                    
                    res.set_content(responseData.dump(), "application/json");
                } else {
                    std::unique_lock<std::mutex> lock(pendingMutex);
                    pendingDirectoryDeletions.erase(dirPath);
                    
                    res.status = 500;
                    res.set_content("Failed to send notification", "text/plain");
                }
            }
        });
        
        // Handle file deletion request
        server.Delete("/file", [this](const httplib::Request& req, httplib::Response& res) {
            if (!req.has_param("path") || !req.has_param("userID")) {
                res.status = 400;
                res.set_content("Missing path or userID parameter", "text/plain");
                return;
            }
            
            std::string filePath = req.get_param_value("path");
            std::string userID = req.get_param_value("userID");
            
            // Get file metadata
            Response resp = get(this->blockServers, filePath);
            if (!resp.success) {
                res.status = 404;
                res.set_content("File not found", "text/plain");
                return;
            }
            
            // Parse file metadata
            json fileMetadata;
            try {
                fileMetadata = json::parse(resp.value);
            } catch (const std::exception& e) {
                res.status = 500;
                res.set_content("Error parsing file metadata", "text/plain");
                return;
            }
            
            // Check owner permission
            if (fileMetadata["owner"] != userID) {
                res.status = 403;
                res.set_content("Permission denied", "text/plain");
                return;
            }
            
            // Extract parent path and filename
            size_t lastSlash = filePath.find_last_of('/');
            if (lastSlash == std::string::npos) {
                res.status = 400;
                res.set_content("Invalid file path", "text/plain");
                return;
            }
            
            std::string parentPath = filePath.substr(0, lastSlash);
            if (parentPath.empty()) {
                parentPath = "/";
            }
            
            std::string fileName = filePath.substr(lastSlash + 1);
            
            // Store deletion info for when confirmation arrives
            {
                std::unique_lock<std::mutex> lock(pendingMutex);
                pendingFileDeletions[filePath] = {
                    {"userID", userID},
                    {"parentPath", parentPath},
                    {"fileName", fileName},
                    {"metadata", fileMetadata}
                };
            }
            
            // Send notification
            json notificationData = {
                {"path", filePath},
                {"name", fileName},
                {"action", "deleted"}
            };
            
            if (sendNotification(userID, "file_deleted", notificationData, true)) {
                json responseData = {
                    {"status", "deletion_initiated"},
                    {"path", filePath}
                };
                res.set_content(responseData.dump(), "application/json");
            } else {
                std::unique_lock<std::mutex> lock(pendingMutex);
                pendingFileDeletions.erase(filePath);
                
                res.status = 500;
                res.set_content("Failed to send notification", "text/plain");
            }
        });
        
        // Handle notification confirmation
        server.Post("/notify-confirmation", [this](const httplib::Request& req, httplib::Response& res) {
            try {
                json confirmation = json::parse(req.body);
                
                if (!confirmation.contains("userID") || 
                    !confirmation.contains("type") || 
                    !confirmation.contains("data") ||
                    !confirmation["data"].contains("path")) {
                    res.status = 400;
                    res.set_content("Invalid confirmation format", "text/plain");
                    return;
                }
                
                std::string type = confirmation["type"];
                std::string path = confirmation["data"]["path"];
                
                bool success = false;
                
                if (type == "file_deleted") {
                    success = completeFileDeletion(path);
                } else if (type == "directory_deleted") {
                    success = completeDirectoryDeletion(path);
                } else {
                    res.status = 400;
                    res.set_content("Unknown notification type: " + type, "text/plain");
                    return;
                }
                
                if (success) {
                    res.set_content("Deletion completed successfully", "text/plain");
                } else {
                    res.status = 500;
                    res.set_content("Failed to complete deletion", "text/plain");
                }
            } catch (const std::exception& e) {
                res.status = 400;
                res.set_content(std::string("Error: ") + e.what(), "text/plain");
            }
        });
        
        std::cout << "Starting metadata server on port " << port << std::endl;
        server.listen("0.0.0.0", port);
    }
};

// Block server client implementation
Response set(const std::vector<std::string>& servers, const std::string& key, const std::string& value) {
    Response response;
    for (const auto& server : servers) {
        size_t colonPos = server.find(':');
        if (colonPos == std::string::npos) continue;
        
        std::string host = server.substr(0, colonPos);
        int port = std::stoi(server.substr(colonPos + 1));
        
        httplib::Client client(host, port);
        json body = {{"key", key}, {"value", value}};
        auto res = client.Post("/set", body.dump(), "application/json");
        
        if (res && res->status == 200) {
            response.success = true;
            response.value = res->body;
            return response;
        }
    }
    
    response.success = false;
    response.err = "Failed to set key on all servers";
    return response;
}

Response get(const std::vector<std::string>& servers, const std::string& key) {
    Response response;
    for (const auto& server : servers) {
        size_t colonPos = server.find(':');
        if (colonPos == std::string::npos) continue;
        
        std::string host = server.substr(0, colonPos);
        int port = std::stoi(server.substr(colonPos + 1));
        
        httplib::Client client(host, port);
        auto res = client.Get("/get?key=" + httplib::detail::encode_url(key));
        
        if (res && res->status == 200) {
            response.success = true;
            response.value = res->body;
            return response;
        }
    }
    
    response.success = false;
    response.err = "Failed to get key from all servers";
    return response;
}

Response del(const std::vector<std::string>& servers, const std::string& key) {
    Response response;
    for (const auto& server : servers) {
        size_t colonPos = server.find(':');
        if (colonPos == std::string::npos) continue;
        
        std::string host = server.substr(0, colonPos);
        int port = std::stoi(server.substr(colonPos + 1));
        
        httplib::Client client(host, port);
        auto res = client.Delete("/del?key=" + httplib::detail::encode_url(key));
        
        if (res && res->status == 200) {
            response.success = true;
            response.value = res->body;
            return response;
        }
    }
    
    response.success = false;
    response.err = "Failed to delete key from all servers";
    return response;
}

int main(int argc, char* argv[]) {
    if (argc < 5) {
        std::cerr << "Usage: " << argv[0] << " <port> <self_url> <notification_server_url> <block_server1> [block_server2] ..." << std::endl;
        return 1;
    }
    
    int port = std::stoi(argv[1]);
    std::string selfUrl = argv[2];
    std::string notificationServerUrl = argv[3];
    
    std::vector<std::string> blockServers;
    for (int i = 4; i < argc; i++) {
        blockServers.push_back(argv[i]);
    }
    
    MetadataServer server(port, blockServers, notificationServerUrl, selfUrl);
    return 0;
}
