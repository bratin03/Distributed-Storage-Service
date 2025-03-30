#include "dropbox_client.h"
#include <fstream>
#include <stdexcept>
#include <thread>

// Constructor: Read the config file to obtain the access token and initialize CURL.
DropboxClient::DropboxClient(const std::string &token)
    : accessToken(token), curlHandle(nullptr)
{
  Logger::info("Initializing DropboxClient.");
  // Initialize CURL.
  curl_global_init(CURL_GLOBAL_ALL);
  curlHandle = curl_easy_init();
  if (!curlHandle)
  {
    Logger::error("Failed to initialize CURL in DropboxClient.");
    throw std::runtime_error("Failed to initialize CURL");
  }
}

// Destructor: Cleanup CURL.
DropboxClient::~DropboxClient()
{
  if (curlHandle)
  {
    curl_easy_cleanup(curlHandle);
    Logger::debug("Cleaned up CURL handle.");
  }
  curl_global_cleanup();
  Logger::debug("Cleaned up CURL global resources.");
}

// A helper function that sets up CURL with the provided parameters, performs the request,
// and fills in a DropboxResponse struct.
DropboxResponse DropboxClient::performRequest(const std::string &url,
                                              const std::string &args,
                                              const std::string &data,
                                              struct curl_slist *headers)
{
  Logger::debug("Performing request to URL: " + url);
  DropboxResponse response;
  std::string readBuffer;

  // Set the URL.
  curl_easy_setopt(curlHandle, CURLOPT_URL, url.c_str());
  // Set headers.
  curl_easy_setopt(curlHandle, CURLOPT_HTTPHEADER, headers);
  // If data is provided, set it as POSTFIELDS.
  curl_easy_setopt(curlHandle, CURLOPT_POSTFIELDS, data.c_str());
  // Set a write callback to capture response data.
  curl_easy_setopt(
      curlHandle, CURLOPT_WRITEFUNCTION,
      +[](char *ptr, size_t size, size_t nmemb, void *userdata) -> size_t
      {
        std::string *str = static_cast<std::string *>(userdata);
        size_t totalSize = size * nmemb;
        str->append(ptr, totalSize);
        return totalSize;
      });
  curl_easy_setopt(curlHandle, CURLOPT_WRITEDATA, &readBuffer);

  // Perform the request.
  CURLcode res = curl_easy_perform(curlHandle);
  if (res != CURLE_OK)
  {
    Logger::error("CURL perform failed: " + std::string(curl_easy_strerror(res)));
    response.responseCode = res;
    response.errorMessage = curl_easy_strerror(res);
  }
  else
  {
    long httpCode = 0;
    curl_easy_getinfo(curlHandle, CURLINFO_RESPONSE_CODE, &httpCode);
    response.responseCode = static_cast<int>(httpCode);
    response.content = readBuffer;
    Logger::debug("Received response with HTTP code: " + std::to_string(response.responseCode));
    // Optionally, try to parse JSON metadata.
    try
    {
      response.metadata = nlohmann::json::parse(readBuffer);
      Logger::debug("Parsed JSON metadata successfully.");
    }
    catch (...)
    {
      Logger::warning("Failed to parse JSON metadata from response.");
      response.metadata = nullptr; // or handle the error as needed
    }
  }
  // Free the headers list.
  curl_slist_free_all(headers);
  Logger::debug("Freed CURL headers.");
  return response;
}

// Create File: Upload file content to Dropbox using the "files/upload" endpoint.
DropboxResponse DropboxClient::createFile(const std::string &dropboxPath,
                                          const std::string &fileContent)
{
  Logger::info("Creating file on Dropbox at path: " + dropboxPath);
  std::string url = "https://content.dropboxapi.com/2/files/upload";

  // Build headers.
  struct curl_slist *headers = NULL;
  std::string authHeader = "Authorization: Bearer " + accessToken;
  headers = curl_slist_append(headers, authHeader.c_str());
  headers = curl_slist_append(headers, "Content-Type: application/octet-stream");
  std::string apiArg =
      "{\"path\": \"" + dropboxPath +
      "\", \"mode\": \"add\", \"autorename\": true, \"mute\": false}";
  std::string dropboxArgHeader = "Dropbox-API-Arg: " + apiArg;
  headers = curl_slist_append(headers, dropboxArgHeader.c_str());

  return performRequest(url, apiArg, fileContent, headers);
}

// Delete File: Deletes a file using the "files/delete_v2" endpoint.
DropboxResponse DropboxClient::deleteFile(const std::string &dropboxPath)
{
  Logger::info("Deleting file from Dropbox: " + dropboxPath);
  std::string url = "https://api.dropboxapi.com/2/files/delete_v2";

  struct curl_slist *headers = NULL;
  std::string authHeader = "Authorization: Bearer " + accessToken;
  headers = curl_slist_append(headers, authHeader.c_str());
  headers = curl_slist_append(headers, "Content-Type: application/json");

  // Create JSON request data.
  nlohmann::json j;
  j["path"] = dropboxPath;
  std::string data = j.dump();

  return performRequest(url, "", data, headers);
}

DropboxResponse DropboxClient::listContent(const std::string &dropboxPath)
{
  Logger::info("Listing content in Dropbox path: " + dropboxPath);
  std::string url = "https://api.dropboxapi.com/2/files/list_folder";

  // Set up headers
  struct curl_slist *headers = NULL;
  std::string authHeader = "Authorization: Bearer " + accessToken;
  headers = curl_slist_append(headers, authHeader.c_str());
  headers = curl_slist_append(headers, "Content-Type: application/json");

  // Prepare the initial JSON request
  nlohmann::json j;
  j["path"] = dropboxPath;
  j["recursive"] = true; // Set to true to list all files in subdirectories.
  std::string data = j.dump();

  // Perform the initial request
  DropboxResponse response = performRequest(url, "", data, headers);
  if (response.responseCode != 200)
  {
    Logger::error("Failed to list content at " + dropboxPath + ". Response: " + response.content);
    return response;
  }
  Logger::debug("Initial list folder response: " + response.content);

  // Parse the initial response and collect entries
  nlohmann::json jsonResponse = nlohmann::json::parse(response.content);
  nlohmann::json allEntries = jsonResponse["entries"];

  // Continue retrieving more entries if there are additional pages
  while (jsonResponse.value("has_more", false))
  {
    std::string cursor = jsonResponse["cursor"];
    std::string continueUrl = "https://api.dropboxapi.com/2/files/list_folder/continue";

    // Create JSON for the continue request
    nlohmann::json continueJson;
    continueJson["cursor"] = cursor;
    std::string continueData = continueJson.dump();

    // Perform the continue request
    DropboxResponse continueResponse = performRequest(continueUrl, "", continueData, headers);
    if (continueResponse.responseCode != 200)
    {
      Logger::error("Failed to continue listing folder. Response: " + continueResponse.content);
      break;
    }
    jsonResponse = nlohmann::json::parse(continueResponse.content);
    for (const auto &entry : jsonResponse["entries"])
    {
      allEntries.push_back(entry);
    }
  }

  // Build final aggregated JSON response
  nlohmann::json finalJson;
  finalJson["entries"] = allEntries;
  if (jsonResponse.contains("cursor"))
  {
    finalJson["cursor"] = jsonResponse["cursor"];
  }

  // Update the original response with the aggregated results
  response.content = finalJson.dump();
  response.metadata = finalJson;
  Logger::debug("Final aggregated list content: " + response.content);
  return response;
}

// Modify File: Overwrite file content using the "files/upload" endpoint with mode "update".
DropboxResponse DropboxClient::modifyFile(const std::string &dropboxPath,
                                          const std::string &newContent,
                                          const std::string &rev)
{
  Logger::info("Modifying file at Dropbox path: " + dropboxPath);
  std::string url = "https://content.dropboxapi.com/2/files/upload";

  // Set up headers.
  struct curl_slist *headers = NULL;
  std::string authHeader = "Authorization: Bearer " + accessToken;
  headers = curl_slist_append(headers, authHeader.c_str());
  headers = curl_slist_append(headers, "Content-Type: application/octet-stream");

  // Construct the API argument JSON with mode "update" that includes the revision.
  nlohmann::json apiArgJson;
  apiArgJson["path"] = dropboxPath;
  apiArgJson["mode"] = {{".tag", "update"}, {"update", rev}};
  apiArgJson["autorename"] = false;
  apiArgJson["mute"] = false;
  std::string apiArg = apiArgJson.dump();

  std::string dropboxArgHeader = "Dropbox-API-Arg: " + apiArg;
  headers = curl_slist_append(headers, dropboxArgHeader.c_str());

  return performRequest(url, apiArg, newContent, headers);
}

// Modify Directory: Example using the "files/move_v2" endpoint to move/rename a directory.
DropboxResponse DropboxClient::modifyDirectory(const std::string &dropboxPath,
                                               const nlohmann::json &params)
{
  Logger::info("Modifying directory from Dropbox path: " + dropboxPath);
  std::string url = "https://api.dropboxapi.com/2/files/move_v2";

  struct curl_slist *headers = NULL;
  std::string authHeader = "Authorization: Bearer " + accessToken;
  headers = curl_slist_append(headers, authHeader.c_str());
  headers = curl_slist_append(headers, "Content-Type: application/json");

  // 'params' is assumed to be correctly formed.
  std::string data = params.dump();

  return performRequest(url, "", data, headers);
}

DropboxResponse DropboxClient::readFile(const std::string &dropboxPath)
{
  Logger::info("Reading file from Dropbox path: " + dropboxPath);
  std::string url = "https://content.dropboxapi.com/2/files/download";

  struct curl_slist *headers = NULL;
  std::string authHeader = "Authorization: Bearer " + accessToken;
  headers = curl_slist_append(headers, authHeader.c_str());
  // Set the proper content type for the download endpoint.
  headers = curl_slist_append(headers, "Content-Type: application/octet-stream");
  std::string apiArg = "{\"path\": \"" + dropboxPath + "\"}";
  std::string dropboxArgHeader = "Dropbox-API-Arg: " + apiArg;
  headers = curl_slist_append(headers, dropboxArgHeader.c_str());

  return performRequest(url, apiArg, "", headers);
}

DropboxResponse DropboxClient::createFolder(const std::string &dropboxPath)
{
  Logger::info("Creating folder on Dropbox at path: " + dropboxPath);
  std::string url = "https://api.dropboxapi.com/2/files/create_folder_v2";

  // Set up headers.
  struct curl_slist *headers = NULL;
  std::string authHeader = "Authorization: Bearer " + accessToken;
  headers = curl_slist_append(headers, authHeader.c_str());
  headers = curl_slist_append(headers, "Content-Type: application/json");

  // Prepare JSON data with required parameters.
  nlohmann::json j;
  j["path"] = dropboxPath;
  j["autorename"] = false;
  std::string data = j.dump();

  return performRequest(url, "", data, headers);
}

DropboxResponse DropboxClient::deleteFolder(const std::string &dropboxPath)
{
  Logger::info("Deleting folder from Dropbox: " + dropboxPath);
  std::string url = "https://api.dropboxapi.com/2/files/delete_v2";

  // Set up headers.
  struct curl_slist *headers = NULL;
  std::string authHeader = "Authorization: Bearer " + accessToken;
  headers = curl_slist_append(headers, authHeader.c_str());
  headers = curl_slist_append(headers, "Content-Type: application/json");

  // Prepare JSON data specifying the folder path to delete.
  nlohmann::json j;
  j["path"] = dropboxPath;
  std::string data = j.dump();

  return performRequest(url, "", data, headers);
}

DropboxResponse DropboxClient::getMetadata(const std::string &dropboxPath)
{
  Logger::info("Getting metadata for Dropbox path: " + dropboxPath);
  std::string url = "https://api.dropboxapi.com/2/files/get_metadata";
  struct curl_slist *headers = NULL;
  std::string authHeader = "Authorization: Bearer " + accessToken;
  headers = curl_slist_append(headers, authHeader.c_str());
  headers = curl_slist_append(headers, "Content-Type: application/json");

  // Prepare the JSON request with the required parameters.
  nlohmann::json j;
  j["path"] = dropboxPath;
  j["include_media_info"] = false;
  j["include_deleted"] = false;
  j["include_has_explicit_shared_members"] = false;
  std::string data = j.dump();

  DropboxResponse response = performRequest(url, "", data, headers);

  // Attempt to parse the JSON content into metadata.
  try
  {
    response.metadata = nlohmann::json::parse(response.content);
    Logger::debug("Parsed metadata successfully for: " + dropboxPath);
  }
  catch (const std::exception &e)
  {
    Logger::error("Error parsing metadata for " + dropboxPath + ": " + std::string(e.what()));
    response.errorMessage = e.what();
  }

  return response;
}

DropboxResponse DropboxClient::longpollFolder(const std::string &cursor,
                                              int timeout)
{
  Logger::info("Longpolling for folder changes with cursor: " + cursor);
  std::string url = "https://notify.dropboxapi.com/2/files/list_folder/longpoll";

  // Set up headers. Authorization header is not required for longpoll.
  struct curl_slist *headers = NULL;
  headers = curl_slist_append(headers, "Content-Type: application/json");

  // Prepare JSON request with the cursor and timeout.
  nlohmann::json j;
  j["cursor"] = cursor;
  j["timeout"] = timeout;
  std::string data = j.dump();

  DropboxResponse response = performRequest(url, "", data, headers);

  // Parse the JSON response into metadata for debugging.
  try
  {
    response.metadata = nlohmann::json::parse(response.content);
    Logger::debug("Longpoll response metadata parsed successfully.");
  }
  catch (const std::exception &e)
  {
    Logger::error("Error parsing longpoll metadata: " + std::string(e.what()));
    response.errorMessage = e.what();
  }

  return response;
}

DropboxResponse DropboxClient::continueListing(const std::string &cursor)
{
  Logger::info("Continuing listing folder with cursor: " + cursor);
  std::string continueUrl = "https://api.dropboxapi.com/2/files/list_folder/continue";

  // Set up headers.
  struct curl_slist *headers = NULL;
  std::string authHeader = "Authorization: Bearer " + accessToken;
  headers = curl_slist_append(headers, authHeader.c_str());
  headers = curl_slist_append(headers, "Content-Type: application/json");

  // Prepare JSON request with the cursor.
  nlohmann::json j;
  j["cursor"] = cursor;
  std::string data = j.dump();

  DropboxResponse response = performRequest(continueUrl, "", data, headers);

  // Attempt to parse the response content into metadata.
  try
  {
    response.metadata = nlohmann::json::parse(response.content);
    Logger::debug("Continue listing response metadata parsed successfully.");
  }
  catch (const std::exception &e)
  {
    Logger::error("Error parsing continue listing metadata: " + std::string(e.what()));
    response.errorMessage = e.what();
  }

  return response;
}

// Member function of DropboxClient to monitor events.
// It now accepts a shared pointer to a mutex and locks it before pushing events onto the queue.
void DropboxClient::monitorEvents(const std::string &filename,
                                  std::shared_ptr<std::queue<nlohmann::json>> eventQueue,
                                  std::shared_ptr<std::mutex> mtx)
{
  Logger::info("Starting event monitoring for: " + filename);
  DropboxResponse listResp = this->listContent(filename);
  std::string cursor;
  try
  {
    if (listResp.metadata.contains("cursor"))
    {
      cursor = listResp.metadata["cursor"].get<std::string>();
      Logger::info("Initial cursor for " + filename + ": " + cursor);
    }
    else
    {
      Logger::error("Cursor not found in initial listing for " + filename);
      return;
    }
  }
  catch (const std::exception &e)
  {
    Logger::error("Error parsing initial metadata: " + std::string(e.what()));
    return;
  }

  const int timeout = 90; // seconds

  // Continuous long polling loop.
  while (true)
  {
    Logger::info("Longpolling for changes (timeout " + std::to_string(timeout) + " seconds)...");
    DropboxResponse lpResp = this->longpollFolder(cursor, timeout);
    bool changesDetected = false;
    try
    {
      if (lpResp.metadata.contains("changes"))
        changesDetected = lpResp.metadata["changes"].get<bool>();
    }
    catch (const std::exception &e)
    {
      Logger::error("Error parsing longpoll metadata: " + std::string(e.what()));
    }

    if (changesDetected)
    {
      Logger::info("Changes detected. Fetching updates...");
      DropboxResponse updatesResp = this->continueListing(cursor);
      if (updatesResp.responseCode != 200)
      {
        Logger::error("Failed to fetch update events: " + updatesResp.content);
      }
      else
      {
        Logger::info("Fetched update events: " + updatesResp.content);
      }

      if (updatesResp.metadata.contains("entries") && updatesResp.metadata["entries"].is_array())
      {
        for (auto &entry : updatesResp.metadata["entries"])
        {
          nlohmann::json eventItem;
          std::string tag = entry[".tag"].get<std::string>();

          if (tag == "file")
          {
            eventItem["event_type"] = "update";
            eventItem["item_type"] = "file";
            eventItem["full_path"] = entry["path_display"];
            eventItem["rev"] = entry["rev"];
            eventItem["content_hash"] = entry["content_hash"];
            eventItem["id"] = entry["id"];
            eventItem["client_modified"] = entry["client_modified"];
            eventItem["server_modified"] = entry["server_modified"];
            eventItem["size"] = entry["size"];
          }
          else if (tag == "folder")
          {
            eventItem["event_type"] = "update";
            eventItem["item_type"] = "folder";
            eventItem["full_path"] = entry["path_display"];
            eventItem["id"] = entry["id"];
          }
          else if (tag == "deleted")
          {
            // Handle deleted events.
            eventItem["event_type"] = "delete";
            eventItem["item_type"] = "unknown"; // Use "unknown" or determine if it was a file/folder if possible.
            eventItem["full_path"] = entry["path_display"];
            eventItem["name"] = entry["name"];
          }

          // Lock the mutex before pushing the event onto the queue.
          {
            std::lock_guard<std::mutex> lock(*mtx);
            eventQueue->push(eventItem);
          }
          Logger::debug("Pushed event: " + eventItem.dump());
        }
      }

      try
      {
        if (updatesResp.metadata.contains("cursor"))
        {
          cursor = updatesResp.metadata["cursor"].get<std::string>();
          Logger::info("Updated cursor: " + cursor);
        }
      }
      catch (const std::exception &e)
      {
        Logger::error("Error updating cursor: " + std::string(e.what()));
      }
    }
    else
    {
      Logger::debug("No changes detected.");
    }
  }
}
