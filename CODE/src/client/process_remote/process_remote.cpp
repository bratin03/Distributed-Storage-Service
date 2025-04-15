#include "process_remote.hpp"

namespace process_remote
{
    void process_remote_events(
        std::queue<json> &eventQueue,
        std::mutex &mtx,
        std::condition_variable &cv,
        std::mutex &db_mutex)
    {
        while (true)
        {
            std::unique_lock<std::mutex> lock(mtx);
            cv.wait(lock, [&eventQueue]
                    { return !eventQueue.empty(); });

            while (!eventQueue.empty())
            {
                auto event = eventQueue.front();
                eventQueue.pop();
                db_mutex.lock();
                process_event(event);
                db_mutex.unlock();
            }
        }
    }

    void process_event(const json &event)
    {
        MyLogger::info("Processing remote event: " + event.dump());
        if (event.is_boolean() && event == false)
        {
            MyLogger::info("Timed out event received, no processing required.");
            return;
        }

        std::string device_id = event["device_id"];
        if (device_id == serverUtils::device_id)
        {
            MyLogger::info("Ignoring event from the same device: " + device_id);
            return;
        }
        std::string type = event["type"];
        std::string path = event["path"];
        if (type == "DIR+")
        {
            fsUtils::ensureDirectoryExists(path);
            metadata::Directory_Metadata dirMetadata(path);
            if (dirMetadata.loadFromDatabase())
            {
                MyLogger::info("Directory already exists in database: " + path);
            }
            else
            {
                dirMetadata.storeToDatabase();
                metadata::addDirectoryToDirectory(path);
            }
            MyLogger::info("Directory created: " + path);
        }
        else if (type == "DIR-")
        {
            fsUtils::removeEntry(path);
            metadata::removeDirectoryFromDatabase(path);
            metadata::removeDirectoryFromDirectory(path);
            MyLogger::info("Directory removed: " + path);
        }
        else if (type == "FILE+" || type == "FILE~")
        {
            metadata::File_Metadata fileMetadata(path);
            if (fileMetadata.loadFromDatabase())
            {
                serverUtils::Conflict(path);
                return;
            }
            else
            {
                auto endpoints = serverUtils::getFileEndpoints(path);
                if (endpoints.empty())
                {
                    MyLogger::error("No endpoints found for file: " + path);
                    return;
                }
                auto response = distributed_KV::getFile(endpoints, path, login::token);
                if (response.success)
                {
                    json response_json = json::parse(response.value);
                    // data and version_number are the keys in the JSON object
                    if (response_json.contains("data") && response_json.contains("version_number"))
                    {
                        std::string file_content = response_json["data"];
                        std::string version_number = response_json["version_number"];
                        fsUtils::createTextFile(path, file_content);
                        fileMetadata.file_content = file_content;
                        fileMetadata.version = version_number;
                        fileMetadata.content_hash = fsUtils::computeSHA256Hash(file_content);
                        fileMetadata.fileSize = file_content.size();
                        if (!fileMetadata.storeToDatabase())
                        {
                            MyLogger::error("Failed to save file metadata to database for: " + path);
                            return;
                        }
                        MyLogger::info("File metadata updated in database for: " + path);
                    }
                    else
                    {
                        MyLogger::error("Invalid response format for file: " + path);
                    }
                }
                else
                {
                    MyLogger::error("Failed to get file from endpoints: " + response.err);
                    return;
                }
            }
        }
        else if (type == "FILE-")
        {
            fsUtils::removeEntry(path);
            metadata::removeFileFromDatabase(path);
            metadata::removeFileFromDirectory(path);
            MyLogger::info("File removed: " + path);
        }
        else
        {
            MyLogger::error("Unknown event type: " + type);
        }
    }
}