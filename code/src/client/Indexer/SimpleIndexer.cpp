
// File: SimpleIndexer.cpp
#include "Indexer.hpp"
#include <iostream>
#include <filesystem>
#include <nlohmann/json.hpp>

namespace fs = std::filesystem;
using json = nlohmann::json;

// Indexer implementation
Indexer::Indexer(const std::string &root, const std::string &meta_dir)
    : root_dir(root), metadata_dir(meta_dir)
{
    // Create metadata directory if it doesn't exist
    fs::create_directories(metadata_dir);
}

void Indexer::scanDirectory(const std::string &dir_path)
{
    std::lock_guard<std::mutex> lock(metadata_mutex);

    // Get relative path from root
    fs::path rel_path = fs::relative(dir_path, root_dir);
    std::string dir_id = rel_path.string();
    if (dir_id.empty())
        dir_id = "."; // Root directory

    // Create or update directory metadata
    DirectoryMetadata dir_meta;
    dir_meta.dir_id = dir_id;
    dir_meta.name = fs::path(dir_path).filename().string();

    // Clear existing entries (we'll rebuild them)
    dir_meta.subdirs.clear();
    dir_meta.files.clear();

    // Scan directory contents
    for (const auto &entry : fs::directory_iterator(dir_path))
    {
        fs::path entry_path = entry.path();

        // Skip .dss directory and hidden files
        if (entry_path.filename().string().starts_with("."))
        {
            continue;
        }

        fs::path rel_entry_path = fs::relative(entry_path, root_dir);
        std::string entry_id = rel_entry_path.string();

        if (fs::is_directory(entry_path))
        {
            // Add subdir to current directory's metadata
            dir_meta.subdirs.push_back(entry_id);

            // Recursively scan subdirectory
            scanDirectory(entry_path.string());
        }
        else if (fs::is_regular_file(entry_path))
        {
            // Add file to current directory's metadata
            dir_meta.files.push_back(entry_id);

            // Update file metadata
            updateFileMetadata(entry_path.string());
        }
    }

    // Save directory metadata
    directory_metadata[dir_id] = dir_meta;
}

void Indexer::updateFileMetadata(const std::string &file_path)
{
    // Get relative path from root
    fs::path rel_path = fs::relative(file_path, root_dir);
    std::string file_id = rel_path.string();

    // Check if we already have metadata for this file
    bool file_exists = file_metadata.find(file_id) != file_metadata.end();

    FileMetadata file_meta;
    if (file_exists)
    {
        file_meta = file_metadata[file_id];
        // Increment version number if file was modified
        auto last_write_time = fs::last_write_time(file_path);
        auto last_write_time_t = std::chrono::duration_cast<std::chrono::seconds>(
                                     last_write_time.time_since_epoch())
                                     .count();

        if (last_write_time_t > file_meta.timestamp)
        {
            file_meta.version_number++;
        }
    }
    else
    {
        // Initialize new file metadata
        file_meta.file_id = file_id;
        file_meta.version_number = 1;
    }

    // Update basic metadata
    file_meta.name = fs::path(file_path).filename().string();
    file_meta.type = fs::path(file_path).extension().string();

    // Update timestamp
    auto last_write_time = fs::last_write_time(file_path);
    file_meta.timestamp = std::chrono::duration_cast<std::chrono::seconds>(
                              last_write_time.time_since_epoch())
                              .count();

    // Status will be updated during sync
    file_meta.status = "complete";

    // Save file metadata
    file_metadata[file_id] = file_meta;
}

void Indexer::updateDirectoryMetadata(const std::string &dir_path)
{
    // Get relative path from root
    fs::path rel_path = fs::relative(dir_path, root_dir);
    std::string dir_id = rel_path.string();
    if (dir_id.empty())
        dir_id = "."; // Root directory

    // Check if directory exists in our metadata
    if (directory_metadata.find(dir_id) == directory_metadata.end())
    {
        DirectoryMetadata dir_meta;
        dir_meta.dir_id = dir_id;
        dir_meta.name = fs::path(dir_path).filename().string();
        directory_metadata[dir_id] = dir_meta;
    }

    // Update parent directory metadata
    fs::path parent_path = fs::path(dir_path).parent_path();
    if (parent_path != root_dir && !fs::equivalent(parent_path, root_dir))
    {
        std::string parent_id = fs::relative(parent_path, root_dir).string();
        if (parent_id.empty())
            parent_id = "."; // Root directory

        if (directory_metadata.find(parent_id) != directory_metadata.end())
        {
            auto &parent_meta = directory_metadata[parent_id];

            // Add this directory to parent's subdirs if not present
            if (std::find(parent_meta.subdirs.begin(), parent_meta.subdirs.end(), dir_id) == parent_meta.subdirs.end())
            {
                parent_meta.subdirs.push_back(dir_id);
            }
        }
    }
}

void Indexer::removeFile(const std::string &file_path)
{
    std::lock_guard<std::mutex> lock(metadata_mutex);

    // Get relative path from root
    fs::path rel_path = fs::relative(file_path, root_dir);
    std::string file_id = rel_path.string();

    // Remove file from metadata
    file_metadata.erase(file_id);

    // Update parent directory metadata
    fs::path parent_path = fs::path(file_path).parent_path();
    std::string parent_id = fs::relative(parent_path, root_dir).string();
    if (parent_id.empty())
        parent_id = "."; // Root directory

    if (directory_metadata.find(parent_id) != directory_metadata.end())
    {
        auto &parent_meta = directory_metadata[parent_id];

        // Remove file from parent's files list
        auto it = std::find(parent_meta.files.begin(), parent_meta.files.end(), file_id);
        if (it != parent_meta.files.end())
        {
            parent_meta.files.erase(it);
        }
    }
}

void Indexer::removeDirectory(const std::string &dir_path)
{
    std::lock_guard<std::mutex> lock(metadata_mutex);

    // Get relative path from root
    fs::path rel_path = fs::relative(dir_path, root_dir);
    std::string dir_id = rel_path.string();

    // Get directory metadata
    if (directory_metadata.find(dir_id) != directory_metadata.end())
    {
        auto &dir_meta = directory_metadata[dir_id];

        // Recursively remove all subdirectories
        for (const auto &subdir_id : dir_meta.subdirs)
        {
            std::string subdir_path = root_dir + "/" + subdir_id;
            removeDirectory(subdir_path);
        }

        // Remove all files in the directory
        for (const auto &file_id : dir_meta.files)
        {
            file_metadata.erase(file_id);
        }

        // Remove directory from metadata
        directory_metadata.erase(dir_id);

        // Update parent directory metadata
        fs::path parent_path = fs::path(dir_path).parent_path();
        std::string parent_id = fs::relative(parent_path, root_dir).string();
        if (parent_id.empty())
            parent_id = "."; // Root directory

        if (directory_metadata.find(parent_id) != directory_metadata.end())
        {
            auto &parent_meta = directory_metadata[parent_id];

            // Remove directory from parent's subdirs list
            auto it = std::find(parent_meta.subdirs.begin(), parent_meta.subdirs.end(), dir_id);
            if (it != parent_meta.subdirs.end())
            {
                parent_meta.subdirs.erase(it);
            }
        }
    }
}

FileMetadata Indexer::getFileMetadata(const std::string &file_id)
{
    std::lock_guard<std::mutex> lock(metadata_mutex);

    if (file_metadata.find(file_id) != file_metadata.end())
    {
        return file_metadata[file_id];
    }

    return FileMetadata(); // Return empty metadata if not found
}

DirectoryMetadata Indexer::getDirMetadata(const std::string &dir_id)
{
    std::lock_guard<std::mutex> lock(metadata_mutex);

    if (directory_metadata.find(dir_id) != directory_metadata.end())
    {
        return directory_metadata[dir_id];
    }

    return DirectoryMetadata(); // Return empty metadata if not found
}

void Indexer::saveMetadata()
{
    std::lock_guard<std::mutex> lock(metadata_mutex);

    // Save directory metadata
    json dir_root = json::object();
    for (const auto &[dir_id, dir_meta] : directory_metadata)
    {
        json dir_json;
        dir_json["dir_id"] = dir_meta.dir_id;
        dir_json["name"] = dir_meta.name;

        json subdirs_json = json::array();
        for (const auto &subdir : dir_meta.subdirs)
        {
            subdirs_json.push_back(subdir);
        }
        dir_json["subdirs"] = subdirs_json;

        json files_json = json::array();
        for (const auto &file : dir_meta.files)
        {
            files_json.push_back(file);
        }
        dir_json["files"] = files_json;

        dir_root[dir_id] = dir_json;
    }

    std::ofstream dir_file(metadata_dir + "/directories.json");
    dir_file << dir_root.dump(4);
    dir_file.close();

    // Save file metadata
    json file_root = json::object();
    for (const auto &[file_id, file_meta] : file_metadata)
    {
        json file_json;
        file_json["file_id"] = file_meta.file_id;
        file_json["name"] = file_meta.name;
        file_json["type"] = file_meta.type;
        file_json["version_number"] = file_meta.version_number;
        file_json["timestamp"] = file_meta.timestamp;
        file_json["overall_hash"] = file_meta.overall_hash;
        file_json["status"] = file_meta.status;

        json chunks_json = json::array();
        for (const auto &chunk : file_meta.chunks)
        {
            json chunk_json;
            chunk_json["chunk_id"] = chunk.chunk_id;
            chunk_json["offset"] = chunk.offset;
            chunk_json["size"] = chunk.size;
            chunks_json.push_back(chunk_json);
        }
        file_json["chunks"] = chunks_json;

        file_root[file_id] = file_json;
    }

    std::ofstream file_file(metadata_dir + "/files.json");
    file_file << file_root.dump(4);
    file_file.close();
}

void Indexer::loa
