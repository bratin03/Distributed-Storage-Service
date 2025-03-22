
// File: Chunker.hpp
#pragma once
#include <vector>
#include <string>
#include "DB/db.hpp"

// Chunker class to split and reassemble files
class Chunker
{
public:
    std::vector<ChunkMetadata> splitFile(const std::string &file_path, const std::string &output_dir);
    void reassembleFile(const std::string &output_path, const std::vector<ChunkMetadata> &chunks, const std::string &chunks_dir);
    std::string calculateChunkHash(const char *data, size_t size);
    std::string calculateFileHash(const std::vector<ChunkMetadata> &chunks);
};
