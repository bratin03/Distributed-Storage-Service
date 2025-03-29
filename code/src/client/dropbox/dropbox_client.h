#ifndef DROPBOX_CLIENT_H
#define DROPBOX_CLIENT_H

#include "../metadata_dropbox/metadata.h"
#include <curl/curl.h>
#include <nlohmann/json.hpp>
#include <rocksdb/db.h>
#include <string>

// Struct to hold a Dropbox response.
struct DropboxResponse {
  int responseCode;         // HTTP response code or CURL error code.
  std::string errorMessage; // Error message, if any.
  std::string content; // Content of the response (e.g. file content or API JSON
                       // result).
  nlohmann::json metadata; // Parsed JSON metadata (if applicable).
};

class DropboxClient {
public:
  // Constructor that takes the path to a config file and reads the access
  // token.
  DropboxClient(const std::string &configPath);
  ~DropboxClient();

  // Dropbox operations.
  DropboxResponse createFile(const std::string &dropboxPath,
                             const std::string &fileContent);
  DropboxResponse deleteFile(const std::string &dropboxPath);
  DropboxResponse listContent(const std::string &dropboxPath);
  // Note: modifyFile now takes a revision parameter.
  DropboxResponse modifyFile(const std::string &dropboxPath,
                             const std::string &newContent,
                             const std::string &rev);
  // For modifyDirectory, we pass a JSON object for parameters (e.g. moving or
  // renaming).
  DropboxResponse modifyDirectory(const std::string &dropboxPath,
                                  const nlohmann::json &params);
  DropboxResponse readFile(const std::string &dropboxPath);
  // New method to get file metadata.
  DropboxResponse getMetadata(const std::string &dropboxPath);
  DropboxResponse createFolder(const std::string &dropboxPath);
  DropboxResponse deleteFolder(const std::string &dropboxPath);

  // New method: Longpoll folder changes using the files/list_folder/longpoll
  // endpoint. cursor: The cursor from a previous list_folder call. timeout:
  // Maximum time (in seconds) to wait for changes.
  DropboxResponse longpollFolder(const std::string &cursor, int timeout = 30);
  // Add this declaration in the public section of DropboxClient:
  DropboxResponse continueListing(const std::string &cursor);

private:
  std::string accessToken;
  CURL *curlHandle;

  // Helper function to perform an HTTP request.
  DropboxResponse performRequest(const std::string &url,
                                 const std::string &args,
                                 const std::string &data,
                                 struct curl_slist *headers);
};

void bootup_1(const nlohmann::json &config, std::shared_ptr<rocksdb::DB> db);

#endif // DROPBOX_CLIENT_H
