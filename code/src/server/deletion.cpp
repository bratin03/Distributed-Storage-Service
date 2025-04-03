// metadata_server.cpp
#include <iostream>
#include <string>
#include <vector>
#include "httplib.h"
#include "json.hpp"
#include "block_server_api.h"

using json = nlohmann::json;
using namespace std;

// List of block server addresses.
std::vector<std::string> database_servers = {
    "http://127.0.0.1:5000",
    "http://127.0.0.1:5001",
    "http://127.0.0.1:5002",
    "http://127.0.0.1:5003",
    "http://127.0.0.1:5004"
};

// List of block server addresses.
std::vector<std::string> notification_servers = {
    "http://127.0.0.1:5005",
    "http://127.0.0.1:5006",
    "http://127.0.0.1:5007",
    "http://127.0.0.1:5008",
    "http://127.0.0.1:5009"
};

// Helper function to check if a directory is empty.
bool isDirectoryEmpty(const json &dirMeta) {
    return dirMeta["subdirectories"].empty() && dirMeta["files"].empty();
}

// Update parent's metadata for a directory deletion.
void updateParentDirectoryForDir(const string &parentPath, const string &dirName) {
    Response parentResp = get(servers, parentPath);
    if (!parentResp.success) {
        cerr << "Failed to get parent's metadata for " << parentPath << endl;
        return;
    }
    try {
        json parentMeta = json::parse(parentResp.value);
        if (parentMeta.contains("subdirectories")) {
            parentMeta["subdirectories"].erase(dirName);
        }
        Response setResp = set(servers, parentPath, parentMeta.dump());
        if (!setResp.success) {
            cerr << "Failed to update parent's metadata for " << parentPath << endl;
        }
    } catch(exception &e) {
        cerr << "Error parsing parent's metadata: " << e.what() << endl;
    }
}

// Update parent's metadata for a file deletion.
void updateParentDirectoryForFile(const string &parentPath, const string &fileName) {
    Response parentResp = get(servers, parentPath);
    if (!parentResp.success) {
        cerr << "Failed to get parent's metadata for " << parentPath << endl;
        return;
    }
    try {
        json parentMeta = json::parse(parentResp.value);
        if (parentMeta.contains("files")) {
            parentMeta["files"].erase(fileName);
        }
        Response setResp = set(servers, parentPath, parentMeta.dump());
        if (!setResp.success) {
            cerr << "Failed to update parent's metadata for " << parentPath << endl;
        }
    } catch(exception &e) {
        cerr << "Error parsing parent's metadata: " << e.what() << endl;
    }
}

// Function to delete a directory. 
void handleDeleteDirectory(const httplib::Request& req, httplib::Response& res)(const httplib::Request &req, httplib::Response &res) {
    // std::string userID;
    // if (!authenticate_request(req, res, userID)) return;

    // Check if the request has a path in body parameter.
    if (!req.has_param("path")) {
        res.status = 400;
        res.set_content("Missing directory path parameter", "text/plain");
        return;
    }
    // Extract the directory path from the request.
    std::string dirPath = req.get_param_value("path");
    std::string parentPath = dirPath.substr(0, dirPath.find_last_of('/'));
    std::string key = userID + ":" + dirPath;

    Response getResp = get(servers, key);
    if (!getResp.success) {
        res.status = 404;
        res.set_content("Directory metadata not found", "text/plain");
        return;
    }

    json dirMeta;
    try {
        dirMeta = json::parse(getResp.value);
    } catch(exception &e) {
        res.status = 500;
        res.set_content(string("Error parsing metadata JSON: ") + e.what(), "text/plain");
        return;
    }
    // Check if the status is already "delete".
    if (dirMeta["status"] == "delete") {
        res.status = 400;
        res.set_content("Directory is already marked for deletion", "text/plain");
        return;
    }

    // Update status to "delete" and save metadata.
    dirMeta["status"] = "delete";
    Response setResp = set(servers, dirPath, dirMeta.dump());
    if (!setResp.success) {
        res.status = 500;
        res.set_content("Failed to update directory metadata status", "text/plain");
        return;
    }


    // Send deletion notification to the notification server.
    httplib::Client notifClient("127.0.0.1", 9090);
    json notifPayload;
    notifPayload["type"] = "directory";
    notifPayload["path"] = dirPath;

    auto notifRes = notifClient.Post("/notifyDeletion", notifPayload.dump(), "application/json");
    if (notifRes && notifRes->status == 200) {
        res.set_content("Directory deletion request accepted", "text/plain");
    } else {
        res.status = 500;
        res.set_content("Failed to contact notification server", "text/plain");
    }
}

void handleFileDeletion(const httplib::Request &req, httplib::Response &res) {

    if (!req.has_param("path")) {
        res.status = 400;
        res.set_content("Missing file path parameter", "text/plain");
        return;
    }
    string filePath = req.get_param_value("path");
    Response getResp = get(servers, filePath);
    if (!getResp.success) {
        res.status = 404;
        res.set_content("File metadata not found", "text/plain");
        return;
    }
    json fileMeta;
    try {
        fileMeta = json::parse(getResp.value);
    } catch(exception &e) {
        res.status = 500;
        res.set_content(string("Error parsing file metadata JSON: ") + e.what(), "text/plain");
        return;
    }

    // Check if the status is already "delete".
    if (fileMeta["status"] == "delete") {
        res.status = 400;
        res.set_content("File is already marked for deletion", "text/plain");
        return;
    }

    // Update file status to "delete" and save the metadata.
    fileMeta["status"] = "delete";
    Response setResp = set(servers, filePath, fileMeta.dump());
    if (!setResp.success) {
        res.status = 500;
        res.set_content("Failed to update file metadata status", "text/plain");
        return;
    }

    // Send deletion notification to the notification server.
    httplib::Client notifClient("127.0.0.1", 9090);
    json notifPayload;
    notifPayload["type"] = "file";
    notifPayload["path"] = filePath;
    
    auto notifRes = notifClient.Post("/notifyDeletion", notifPayload.dump(), "application/json");
    if (notifRes && notifRes->status == 200) {
        res.set_content("File deletion request accepted", "text/plain");
    } else {
        res.status = 500;
        res.set_content("Failed to contact notification server", "text/plain");
    }
}

void handleNotificationDeleteDirectory(const httplib::Request &req, httplib::Response &res) {
    try {
        auto payload = json::parse(req.body);
        if (!payload.contains("directory")) {
            res.status = 400;
            res.set_content("Missing directory in payload", "text/plain");
            return;
        }
        string dirPath = payload["directory"];
        // Delete the directory metadata from the block server.
        Response delResp = del(servers, dirPath);
        if (!delResp.success) {
            res.status = 500;
            res.set_content("Failed to delete directory metadata", "text/plain");
            return;
        }
        // Update the parent directory metadata if provided.
        if (payload.contains("parent")) {
            string parentPath = payload["parent"];
            string dirName = dirPath.substr(dirPath.find_last_of('/') + 1);
            updateParentDirectoryForDir(parentPath, dirName);
        }
        res.set_content("Directory deletion confirmed", "text/plain");
    } catch(exception &e) {
        res.status = 400;
        res.set_content(string("Error parsing JSON: ") + e.what(), "text/plain");
    }
}

void handleNotificationDeleteFile(const httplib::Request &req, httplib::Response &res) {
    try {
        auto payload = json::parse(req.body);
        if (!payload.contains("file")) {
            res.status = 400;
            res.set_content("Missing file in payload", "text/plain");
            return;
        }
        string filePath = payload["file"];
        // Delete the file metadata from the block server.
        Response delResp = del(servers, filePath);
        if (!delResp.success) {
            res.status = 500;
            res.set_content("Failed to delete file metadata", "text/plain");
            return;
        }
        // Update the parent directory metadata if provided.
        if (payload.contains("parent")) {
            string parentPath = payload["parent"];
            string fileName = filePath.substr(filePath.find_last_of('/') + 1);
            updateParentDirectoryForFile(parentPath, fileName);
        }
        res.set_content("File deletion confirmed", "text/plain");
    } catch(exception &e) {
        res.status = 400;
        res.set_content(string("Error parsing JSON: ") + e.what(), "text/plain");
    }
}

int main() {
    httplib::Server svr;

    // DELETE /deleteDirectory endpoint.
    svr.Delete("/deleteDirectory", handleDeleteDirectory);

    // DELETE /deleteFile endpoint.
    svr.Delete("/deleteFile", handleFileDeletion);

    // POST /confirmNotification/delete_dir endpoint.
    svr.Post("/confirmNotification/delete_dir", handleNotificationDeleteDirectory);

    // POST /confirmNotification/delete_file endpoint.
    svr.Post("/confirmNotification/delete_file", handleNotificationDeleteFile);

    cout << "Metadata server running on port 8081..." << endl;
    svr.listen("0.0.0.0", 8081);
    return 0;
}
