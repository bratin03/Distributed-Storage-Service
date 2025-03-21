#include "APISynchronizer.hpp"
#include <iostream>
#include <sstream>
#include <thread>
#include <chrono>

// ----- Dummy HTTP Client ----- //
// In practice, replace this with a real HTTP client library.
class HttpClient {
public:
    // Simulated GET request.
    std::string get(const std::string& url) {
        std::cout << "[HTTP GET] " << url << std::endl;
        // Return a dummy success response.
        return "{\"status\":\"200 OK\", \"data\":\"dummy_response\"}";
    }
    
    // Simulated POST request.
    std::string post(const std::string& url, const std::string& payload) {
        std::cout << "[HTTP POST] " << url << std::endl;
        std::cout << "Payload: " << payload << std::endl;
        // Simulate different responses based on endpoint.
        if (url.find("/update_request/") != std::string::npos) {
            return "{\"nb\": [\"chunk_needed_1\", \"chunk_needed_2\"]}";
        } else if (url.find("/commit_update/") != std::string::npos) {
            // For simplicity, always return 200 OK.
            return "{\"status\":\"200 OK\"}";
        }
        return "{\"status\":\"200 OK\"}";
    }
};

static HttpClient httpClient;

// ----- DIRECTORY APIs ----- //

bool APISynchronizer::createDirectory(const std::string& dir_id) {
    std::string url = "/create_directory/" + dir_id;
    std::string response = httpClient.get(url);
    std::cout << "Response: " << response << std::endl;
    // Assume response status is OK.
    return true;
}

bool APISynchronizer::deleteDirectory(const std::string& dir_id) {
    std::string url = "/delete_directory/" + dir_id;
    std::string response = httpClient.get(url);
    std::cout << "Response: " << response << std::endl;
    return true;
}

std::string APISynchronizer::listDirectory(const std::string& dir_id) {
    std::string url = "/list_directory/" + dir_id;
    std::string response = httpClient.get(url);
    std::cout << "Response: " << response << std::endl;
    return response;
}

// ----- FILE APIs ----- //

bool APISynchronizer::deleteFile(const std::string& file_id) {
    std::string url = "/delete_file";
    std::ostringstream oss;
    oss << "{ \"file_id\": \"" << file_id << "\" }";
    std::string payload = oss.str();
    std::string response = httpClient.post(url, payload);
    std::cout << "Response: " << response << std::endl;
    return true;
}

std::string APISynchronizer::downloadFile(const std::string& file_id) {
    std::string url = "/download_file/" + file_id;
    std::string response = httpClient.get(url);
    std::cout << "Response: " << response << std::endl;
    return response;
}

std::string APISynchronizer::downloadChunk(const std::string& chunk_id) {
    std::string url = "/download_chunk/" + chunk_id;
    std::string response = httpClient.get(url);
    std::cout << "Response: " << response << std::endl;
    return response;
}

// ----- UPDATE FILE APIs ----- //

std::string APISynchronizer::updateRequest(const std::string& file_version, 
                                             const std::string& file_metadata, 
                                             const std::vector<std::string>& chunkIDs) {
    std::string url = "/update_request/";
    std::ostringstream oss;
    oss << "{"
        << "\"file_version\": \"" << file_version << "\","
        << "\"file_metadata\": " << file_metadata << ","
        << "\"chunkIDs\": [";
    for (size_t i = 0; i < chunkIDs.size(); ++i) {
        oss << "\"" << chunkIDs[i] << "\"";
        if (i != chunkIDs.size() - 1) {
            oss << ",";
        }
    }
    oss << "]"
        << "}";
    std::string payload = oss.str();
    std::string response = httpClient.post(url, payload);
    std::cout << "Response: " << response << std::endl;
    return response;
}

bool APISynchronizer::storeChunk(const std::string& chunkid, 
                                 const std::string& metadata, 
                                 const std::string& data) {
    std::string url = "/store_chunk/";
    std::ostringstream oss;
    oss << "{"
        << "\"chunkid\": \"" << chunkid << "\","
        << "\"metadata\": " << metadata << ","
        << "\"data\": \"" << data << "\""
        << "}";
    std::string payload = oss.str();
    std::string response = httpClient.post(url, payload);
    std::cout << "Response: " << response << std::endl;
    return true;
}

std::string APISynchronizer::commitUpdate(const std::string& file_metadata, 
                                          const std::vector<std::string>& chunkIDs) {
    std::string url = "/commit_update/";
    std::ostringstream oss;
    oss << "{"
        << "\"file_metadata\": " << file_metadata << ","
        << "\"chunkIDs\": [";
    for (size_t i = 0; i < chunkIDs.size(); ++i) {
        oss << "\"" << chunkIDs[i] << "\"";
        if (i != chunkIDs.size() - 1)
            oss << ",";
    }
    oss << "]"
        << "}";
    std::string payload = oss.str();
    std::string response = httpClient.post(url, payload);
    std::cout << "Response: " << response << std::endl;
    return response;
}
