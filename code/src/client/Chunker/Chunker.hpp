
// File: Chunker.hpp
#pragma once
#include <vector>
#include <string>

// Represents a file chunk
struct Chunk {
    std::string id;    // e.g. SHA-256 hash
    std::vector<char> data;
};

class Chunker {
public:
    // Splits the file at 'filePath' into chunks of fixed size (e.g., 4MB)
    virtual std::vector<Chunk> chunkFile(const std::string& filePath, size_t chunkSize = 4 * 1024 * 1024) = 0;
    // Reassembles chunks back to file content (for download)
    virtual std::vector<char> reassembleFile(const std::vector<Chunk>& chunks) = 0;
    virtual ~Chunker() = default;
};