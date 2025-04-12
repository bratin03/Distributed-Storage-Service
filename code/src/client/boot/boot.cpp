#include "boot.hpp"

namespace boot
{
    void localSync()
    {
        boot::syncDir(fsUtils::g_basePath);
    }
    void syncDir(const fs::path &path)
    {
        // Check if the path is a directory
        if (!std::filesystem::is_directory(path))
        {
            MyLogger::error("Provided path is not a directory: " + path.string());
            return;
        }
        auto directory_key = fsUtils::buildKeyfromFullPath(path);
        MyLogger::debug("Syncing directory: " + path.string() + " with key: " + directory_key);
        metadata::Directory_Metadata dir_metadata(directory_key);
        if (dir_metadata.loadFromDatabase())
        {
            MyLogger::info("Directory metadata loaded from database for: " + directory_key);
        }
        else
        {
            MyLogger::warning("Directory metadata not found in database for: " + directory_key);
        }

        std::vector<std::string> files;
        std::vector<std::string> directories;
        std::set<std::string> file_keys;
        std::set<std::string> dir_keys;
        // Iterate through the directory for txt files and subdirectories
        for (const auto &entry : std::filesystem::directory_iterator(path))
        {
            if (entry.is_regular_file() && entry.path().extension() == ".txt")
            {
                auto file_key = fsUtils::buildKeyfromFullPath(entry.path());
                MyLogger::debug("Found text file: " + entry.path().string() + " with key: " + file_key);
                metadata::File_Metadata file_metadata(file_key);
                if (file_metadata.loadFromDatabase())
                {
                    MyLogger::info("File metadata loaded from database for: " + file_key);
                }
                else
                {
                    MyLogger::warning("File metadata not found in database for: " + file_key);
                    if (file_metadata.storeToDatabase())
                    {
                        MyLogger::info("File metadata stored to database for: " + file_key);
                    }
                    else
                    {
                        MyLogger::error("Failed to store file metadata to database for: " + file_key);
                    }
                }
                file_keys.insert(file_key);
                files.push_back(file_key);
            }
            else if (entry.is_directory())
            {
                auto dir_key = fsUtils::buildKeyfromFullPath(entry.path());
                MyLogger::debug("Found directory: " + entry.path().string() + " with key: " + dir_key);
                directories.push_back(dir_key);
                dir_keys.insert(dir_key);
                syncDir(entry.path());
            }
        }
        // Update the directory metadata
        dir_metadata.files = files;
        dir_metadata.directories = directories;
        if (dir_metadata.storeToDatabase())
        {
            MyLogger::info("Directory metadata stored to database for: " + directory_key);
        }
        else
        {
            MyLogger::error("Failed to store directory metadata to database for: " + directory_key);
        }
        // Now remove any files or directories that are no longer present
        auto prefix_scan = metadata::prefix_scan(directory_key + (directory_key.back() == '/' ? "" : "/"));
        for (const auto &file_key : prefix_scan)
        {
            MyLogger::debug("Checking file key: " + file_key);
            if (file_keys.find(file_key) == file_keys.end() && dir_keys.find(file_key) == dir_keys.end())
            {
                // Check if the key is a file or directory by seeing .txt at the end
                if (file_key.find(".txt") != std::string::npos)
                {
                    metadata::removeFileFromDatabase(file_key);
                }
                else
                {
                    metadata::removeDirectoryFromDatabase(file_key);
                }
            }
        }
    }
    void localToRemote()
    {
        MyLogger::info("Starting local to remote synchronization...");
        auto dir_key = fsUtils::buildKeyfromFullPath(fsUtils::g_basePath);
        if (dir_key.back() == '/')
            dir_key.pop_back();
        localToRemoteDirCheck(dir_key);
    }
    void localToRemoteDirCheck(const std::string &dir_key)
    {
        MyLogger::info("Checking remote directory: " + dir_key);
        auto resp = login::makeRequest(login::metaLoadBalancerip, login::metaLoadBalancerPort, "/list-directory", json{{"path", dir_key}});
        if (resp == nullptr)
        {
            MyLogger::error("Failed to get response from server for list-directory");
            return;
        }
        MyLogger::info("Response from server: " + resp.dump());
        // in subdirectories field we have the subdirectories, extract the subdirectories
        std::set<std::string> subdirectories;
        if (resp.contains("subdirectories"))
        {
            for (const auto &subdir : resp["subdirectories"])
            {
                subdirectories.insert(subdir);
                MyLogger::info("Found subdirectory in remote: " + subdir.get<std::string>());
            }
        }
        std::set<std::string> files;
        if (resp.contains("files"))
        {
            for (const auto &file : resp["files"])
            {
                files.insert(file);
                MyLogger::info("Found file in remote: " + file.get<std::string>());
            }
        }
        metadata::Directory_Metadata dir_metadata(dir_key);
        if (dir_metadata.loadFromDatabase())
        {
            MyLogger::info("Directory metadata loaded from database for: " + dir_key);
        }
        else
        {
            MyLogger::warning("Directory metadata not found in database for: " + dir_key);
            return;
        }
        // Look for files that are in the database but not in the remote directory
        for (const auto &file_key : dir_metadata.files)
        {
            if (files.find(file_key) == files.end())
            {
                MyLogger::info("File not found in remote directory, sending: " + file_key);
                serverUtils::createFile(file_key);
            }
        }
        // Look for subdirectories that are in the database but not in the remote directory
        for (const auto &subdir_key : dir_metadata.directories)
        {
            if (subdirectories.find(subdir_key) == subdirectories.end())
            {
                MyLogger::info("Subdirectory not found in remote directory, sending: " + subdir_key);
                sendDirRecursively(subdir_key);
            }
            else
            {
                localToRemoteDirCheck(subdir_key);
            }
        }
    }
    void sendDirRecursively(const std::string &dir_key)
    {
        MyLogger::info("Sending directory recursively: " + dir_key);
        serverUtils::createDir(dir_key);
        metadata::Directory_Metadata dir_metadata(dir_key);
        if (dir_metadata.loadFromDatabase())
        {
            MyLogger::info("Directory metadata loaded from database for: " + dir_key);
        }
        else
        {
            MyLogger::warning("Directory metadata not found in database for: " + dir_key);
            return;
        }
        for (const auto &file_key : dir_metadata.files)
        {
            MyLogger::info("Sending file: " + file_key);
            serverUtils::createFile(file_key);
        }

        for (const auto &subdir_key : dir_metadata.directories)
        {
            sendDirRecursively(subdir_key);
        }
    }
}