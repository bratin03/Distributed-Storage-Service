#define CPPHTTPLIB_OPENSSL_SUPPORT
#include "httplib.h"
#include <iostream>
#include <unordered_map>
#include <mutex>
#include <nlohmann/json.hpp> // JSON parsing

using json = nlohmann::json;
using namespace httplib;

// Simulated metadata store
std::unordered_map<std::string, json> directory_metadata;
std::unordered_map<std::string, json> file_metadata;
std::mutex metadata_lock;


/*
    Things to do:
    -> authentication
    -> heartbeat
    -> notification server
    -> logging (if possible)
    -> error handling
    -> connect with DB

*/


void create_directory(const Request &req, Response &res) {
    std::string dir_id = req.matches[1];

    std::lock_guard<std::mutex> lock(metadata_lock);
    if (directory_metadata.count(dir_id)) {
        res.status = 400;
        res.set_content(R"({"error": "Directory already exists"})", "application/json");
        return;
    }

    directory_metadata[dir_id] = json::object();
    res.set_content(R"({"message": "Directory created"})", "application/json");

    // send notification to all clients
}

void delete_directory(const Request &req, Response &res) {
    std::string dir_id = req.matches[1];

    std::lock_guard<std::mutex> lock(metadata_lock);
    if (!directory_metadata.count(dir_id)) {
        res.status = 404;
        res.set_content(R"({"error": "Directory not found"})", "application/json");
        return;
    }

    directory_metadata.erase(dir_id);
    res.set_content(R"({"message": "Directory deleted"})", "application/json");

    // send notification to all clients

}

void list_directory(const Request &req, Response &res) {
    std::string dir_id = req.matches[1];

    std::lock_guard<std::mutex> lock(metadata_lock);
    if (!directory_metadata.count(dir_id)) {
        res.status = 404;
        res.set_content(R"({"error": "Directory not found"})", "application/json");
        return;
    }

    res.set_content(directory_metadata[dir_id].dump(), "application/json");
}

void delete_file(const Request &req, Response &res) {
    std::string file_id = req.matches[1];

    std::lock_guard<std::mutex> lock(metadata_lock);
    if (!file_metadata.count(file_id)) {
        res.status = 404;
        res.set_content(R"({"error": "File not found"})", "application/json");
        return;
    }

    file_metadata.erase(file_id);
    res.set_content(R"({"message": "File deleted"})", "application/json");
}

// void download_file(const Request &req, Response &res) {
//     std::string file_id = req.matches[1];

//     std::lock_guard<std::mutex> lock(metadata_lock);
//     if (!file_metadata.count(file_id)) {
//         res.status = 404;
//         res.set_content(R"({"error": "File not found"})", "application/json");
//         return;
//     }

//     res.set_content(file_metadata[file_id].dump(), "application/json");
// }

// void update_request(const Request &req, Response &res) {
//     try {
//         json request_body = json::parse(req.body);
//         int file_version = request_body["file_version"];
//         json metadata = request_body["metadata"];

//         std::string file_id = metadata["file_id"];

//         std::lock_guard<std::mutex> lock(metadata_lock);
//         file_metadata[file_id] = metadata;

//         json response = {{"message", "Update request received"}, {"nb", json::array()}};
//         res.set_content(response.dump(), "application/json");
//     } catch (std::exception &e) {
//         res.status = 400;
//         res.set_content(R"({"error": "Invalid JSON"})", "application/json");
//     }
// }

// void commit_update(const Request &req, Response &res) {
//     try {
//         json request_body = json::parse(req.body);
//         std::string file_id = request_body["metadata"]["file_id"];

//         std::lock_guard<std::mutex> lock(metadata_lock);
//         if (!file_metadata.count(file_id)) {
//             res.status = 404;
//             res.set_content(R"({"error": "File not found"})", "application/json");
//             return;
//         }

//         res.set_content(R"({"message": "Update committed"})", "application/json");
//     } catch (std::exception &e) {
//         res.status = 400;
//         res.set_content(R"({"error": "Invalid JSON"})", "application/json");
//     }
// }

int main() {
    Server svr;

    // Directory operations
    svr.Post(R"(/create_directory/(\S+))", create_directory);
    svr.Delete(R"(/delete_directory/(\S+))", delete_directory);
    svr.Get(R"(/list_directory/(\S+))", list_directory);

    // File operations
    svr.Delete(R"(/delete_file/(\S+))", delete_file);
    // svr.Get(R"(/download_file/(\S+))", download_file);
    // svr.Post("/update_request", update_request);
    // svr.Post("/commit_update", commit_update);

    std::cout << "Metadata Server running on port 8080..." << std::endl;
    svr.listen("0.0.0.0", 8080);

    return 0;
}
