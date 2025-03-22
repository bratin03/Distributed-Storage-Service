#include <string>
#include <vector>

// Metadata structures based on the specification
struct ChunkMetadata {
    std::string chunk_id;    // SHA256 hash
    size_t offset;           // Order of chunk in file
    size_t size;             // Size in bytes
};

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

struct DirectoryMetadata {
    std::string dir_id;      // Path from root dir
    std::string name;        // Directory name
    std::vector<std::string> subdirs; // List of Dir IDs
    std::vector<std::string> files;   // List of File IDs
};