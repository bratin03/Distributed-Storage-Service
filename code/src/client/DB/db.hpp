#pragma once
#include <string>
#include <vector>
#include <nlohmann/json.hpp>

// Metadata structures based on the specification
struct ChunkMetadata {
    std::string chunk_id;    // SHA256 hash
    size_t offset;           // Order of chunk in file
    size_t size;             // Size in bytes
};

// Define how to serialize ChunkMetadata to JSON
inline void to_json(nlohmann::json& j, const ChunkMetadata& chunk) {
    j = nlohmann::json{
        {"chunk_id", chunk.chunk_id},
        {"offset", chunk.offset},
        {"size", chunk.size}
    };
}

// Define how to deserialize JSON to ChunkMetadata
inline void from_json(const nlohmann::json& j, ChunkMetadata& chunk) {
    j.at("chunk_id").get_to(chunk.chunk_id);
    j.at("offset").get_to(chunk.offset);
    j.at("size").get_to(chunk.size);
}

struct FileMetadata {
    std::string file_id;     // Path from root dir
    std::string name;        // File name
    std::string type;        // File type
    int version_number;      // Incremented on update
    int64_t timestamp;       // Last modified timestamp
    std::string overall_hash;// Aggregate hash of all chunks
    std::vector<ChunkMetadata> chunks; // List of chunks
    std::string status;      // uploading/complete
};

// Define how to serialize FileMetadata to JSON
inline void to_json(nlohmann::json& j, const FileMetadata& metadata) {
    j = nlohmann::json{
        {"file_id", metadata.file_id},
        {"name", metadata.name},
        {"type", metadata.type},
        {"version_number", metadata.version_number},
        {"timestamp", metadata.timestamp},
        {"overall_hash", metadata.overall_hash},
        {"status", metadata.status},
        {"chunks", metadata.chunks} // Ensure ChunkMetadata has its own to_json/from_json
    };
}

// Define how to deserialize JSON to FileMetadata
inline void from_json(const nlohmann::json& j, FileMetadata& metadata) {
    j.at("file_id").get_to(metadata.file_id);
    j.at("name").get_to(metadata.name);
    j.at("type").get_to(metadata.type);
    j.at("version_number").get_to(metadata.version_number);
    j.at("timestamp").get_to(metadata.timestamp);
    j.at("overall_hash").get_to(metadata.overall_hash);
    j.at("status").get_to(metadata.status);
    j.at("chunks").get_to(metadata.chunks); // Ensure ChunkMetadata has its own to_json/from_json
}

struct DirectoryMetadata {
    std::string dir_id;      // Path from root dir
    std::string name;        // Directory name
    std::vector<std::string> subdirs; // List of Dir IDs
    std::vector<std::string> files;   // List of File IDs
};