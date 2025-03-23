

#pragma once
#include <string>
#include <vector>
#include <map>
#include <mutex>
#include "../DB/db.hpp"

// Indexer class to manage metadata
class Indexer {
    private:
        std::string root_dir;
        std::string metadata_dir;
        std::map<std::string, DirectoryMetadata> directory_metadata;
        std::map<std::string, FileMetadata> file_metadata;
        std::mutex metadata_mutex;
    
    public:
        Indexer(const std::string& root, const std::string& meta_dir);
        
        void scanDirectory(const std::string& dir_path);
        void updateFileMetadata(const std::string& file_path);
        void updateDirectoryMetadata(const std::string& dir_path);
        void removeFile(const std::string& file_path);
        void removeDirectory(const std::string& dir_path);
        
        FileMetadata getFileMetadata(const std::string& file_id);
        DirectoryMetadata getDirMetadata(const std::string& dir_id);
        
        void saveMetadata();
        void loadMetadata();
    };
