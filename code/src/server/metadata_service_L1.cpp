#define CPPHTTPLIB_OPENSSL_SUPPORT
#include "../../utils/libraries/cpp-httplib/httplib.h"
#include <iostream>
#include <unordered_map>
#include <mutex>
#include <nlohmann/json.hpp> // JSON parsing
#include <jwt-cpp/jwt.h>
#include<bits/stdc++.h>

using json = nlohmann::json;
using namespace httplib;

// Public key for JWT verification
const std::string PUBLIC_KEY = R"(-----BEGIN PUBLIC KEY-----
...Your Public Key Here...
-----END PUBLIC KEY-----)";

// Simulated metadata store
std::unordered_map<std::string, json> directory_metadata;
std::unordered_map<std::string, json> file_metadata;
std::mutex metadata_lock;


// Static list of metadata server endpoints (to be replaced with distributed store later)
std::vector<std::string> metadata_servers = {"http://server1:8080", "http://server2:8080", "http://server3:8080"};


// Function to verify JWT token and extract userID
std::optional<std::string> verify_jwt(const std::string &token) {
    // Ensure that the PUBLIC_KEY is correctly formatted and valid for the RS256 algorithm.
    // The error message "JWT Verification Failed" could be more descriptive 

      // Ensure the token has three parts
    if (std::count(token.begin(), token.end(), '.') != 2) {
        std::cerr << "JWT Format Error: Incorrect token structure" << std::endl;
        return std::nullopt;
    }

    try {
    
        auto decoded = jwt::decode(token);
        auto verifier = jwt::verify()
            .allow_algorithm(jwt::algorithm::rs256(PUBLIC_KEY))
            .with_issuer("auth-server");

        verifier.verify(decoded);
        return decoded.get_payload_claim("userID").as_string();
    } 
    catch (const jwt::TokenExpiredError &e) {
        std::cerr << "JWT Verification Failed: Token expired" << std::endl;
        return std::nullopt;
    }
    catch (const jwt::SignatureVerificationError &e) {
        std::cerr << "JWT Verification Failed: Invalid signature" << std::endl;
        return std::nullopt;
    }
    catch (const std::exception &e) {
        std::cerr << "JWT Verification Failed: " << e.what() << std::endl;
        return std::nullopt;
    }
}

// Middleware to handle authentication
// This function verifies the JWT token provided in the "Authorization" header of the request.
// If the token is valid, it extracts the user ID and assigns it to the userID parameter.
// If the token is missing, invalid, or expired, it sets the appropriate HTTP response status and error message.
bool authenticate_request(const Request &req, Response &res, std::string &userID) {


    // make sure the format is correct 
    if (!req.has_header("Authorization")) {
        res.status = 401;
        res.set_content(R"({"error": "Missing authentication token"})", "application/json");
        return false;
    }
    
    std::string token = req.get_header_value("Authorization");
    // make sure there is no whitespacing error
    
    // token.erase(0, token.find_first_not_of(" \t\n\r")); // Trim leading whitespace
    // token.erase(token.find_last_not_of(" \t\n\r") + 1); // Trim trailing whitespace 

    if (token.rfind("Bearer ", 0) == 0) {
        // The rfind method with 0 as the second argument is used to check if the token starts with "Bearer "
        token = token.substr(7); // Remove "Bearer " prefix
    }
    
    auto verified_user = verify_jwt(token);
    if (!verified_user) {
        res.status = 403;
        res.set_content(R"({"error": "Invalid token"})", "application/json");
        return false;
    }
    
    userID = *verified_user;
    return true;
}



/*
    Example metadata structure 
    :
    "owner": userID,
    "timestamp": 1690000000, // Unix timestamp
        "subdirectories": {
            "subdir1": ["IP1:Port1", "IP2:Port2"],
            "subdir2": ["IP3:Port3"]
        },
        "files": {
            "file1.txt": ["IP4:Port4"],
            "file2.txt": ["IP5:Port5", "IP6:Port6"]
        },
        "endpoints": ["IP1:Port1", "IP2:Port2", "IP3:Port3"]

    Client will need to parse this metadata to get the list of subdirectories and files end points then hit those end points    
*/
// Function to handle directory creation
void create_directory(const Request &req, Response &res) {
    std::string userID;
    if (!authenticate_request(req, res, userID)) return;
    
    std::string dir_id = req.matches[1];
    std::string key = userID + ":" + dir_id;
    
    std::lock_guard<std::mutex> lock(metadata_lock);

    // Check if the directory already exists
    if (directory_metadata.count(key)) {
        res.status = 400;
        res.set_content(R"({"error": "Directory already exists"})", "application/json");
        return;
    }

    // Check if the immediate parent directory exists
    size_t last_slash = dir_id.find_last_of('/');
    std::string parent_dir, parent_key;
    if (last_slash != std::string::npos) {
        parent_dir = dir_id.substr(0, last_slash);
        parent_key = userID + ":" + parent_dir;
        
        if (!directory_metadata.count(parent_key)) {
            res.status = 404;
            res.set_content(R"({"error": "Parent directory not found"})", "application/json");
            return;
        }

        // Modify parent metadata to include the new subdirectory
        directory_metadata[parent_key]["subdirectories"][dir_id] = json::array();  // Empty list for now
    }

    // Select 3 metadata servers (for now from the static list)
    std::vector<std::string> chosen_servers(metadata_servers.begin(), metadata_servers.begin() + 3);
    
    // Store the new directory's metadata
    directory_metadata[key] = {
        {"owner", userID},
        {"timestamp", std::chrono::system_clock::to_time_t(std::chrono::system_clock::now())},
        {"subdirectories", json::object()},
        {"files", json::object()},
        {"endpoints", chosen_servers},
        {"parent_dir", parent_dir}        
    };

    // Update parent metadata with chosen endpoints
    if (!parent_key.empty()) {  // Since parent_key was already set, just check if it exists
        directory_metadata[parent_key]["subdirectories"][dir_id] = chosen_servers;
    }

    res.set_content(R"({"message": "Directory created", "metadata": )" + metadata.dump() + "}", "application/json");
}




void list_directory(const Request &req, Response &res) {
    std::string userID;
    if (!authenticate_request(req, res, userID)) return;
    
    std::string dir_id = req.matches[1];
    std::string key = userID + ":" + dir_id;
    
    std::lock_guard<std::mutex> lock(metadata_lock);
    if (!directory_metadata.count(key)) {
        res.status = 404;
        res.set_content(R"({"error": "Directory not found"})", "application/json");
        return;
    }
    
    res.set_content(directory_metadata[key].dump(), "application/json");
}

void delete_file(const Request &req, Response &res) {
    std::string userID;
    if (!authenticate_request(req, res, userID)) return;
    
    std::string file_id = req.matches[1];
    std::string key = userID + ":" + file_id;
    
    std::lock_guard<std::mutex> lock(metadata_lock);
    if (!file_metadata.count(key)) {
        res.status = 404;
        res.set_content(R"({"error": "File not found"})", "application/json");
        return;
    }
    
    file_metadata.erase(key);
    res.set_content(R"({"message": "File deleted"})", "application/json");
}

void delete_directory(const Request &req, Response &res) {
    std::string userID;
    if (!authenticate_request(req, res, userID)) return;
    
    std::string dir_id = req.matches[1];
    std::string key = userID + ":" + dir_id;
    
    std::lock_guard<std::mutex> lock(metadata_lock);
    if (!directory_metadata.count(key)) {
        res.status = 404;
        res.set_content(R"({"error": "Directory not found"})", "application/json");
        return;
    }
    
    directory_metadata.erase(key);
    res.set_content(R"({"message": "Directory deleted"})", "application/json");
}


// void heartbeat_handler(const Request &, Response &res) {
//     res.set_content(R"({"status": "alive"})", "application/json");
// }

int main() {
    Server svr;

    // Directory operations
    svr.Post(R"(/create_directory/([\w-]+))", create_directory);
    svr.Delete(R"(/delete_directory/([\w-]+))", delete_directory);
    svr.Get(R"(/list_directory/([\w-]+))", list_directory);
    
    // File operations
    svr.Delete(R"(/delete_file/([\w-]+))", delete_file);

    // Heartbeat
    // svr.Get("/heartbeat", heartbeat_handler);
    
    std::cout << "Metadata Server running on port 8080..." << std::endl;
    svr.listen("0.0.0.0", 8080);

    return 0;
}
