


// File: Indexer.hpp
#pragma once
#include <string>
#include <vector>
#include "Chunker.hpp"

// A simplified interface for indexing files and chunks
class Indexer {
public:
    // Updates the metadata for a file when its chunks are re-calculated
    virtual void updateFileIndex(const std::string& filePath, const std::vector<Chunk>& chunks) = 0;
    virtual ~Indexer() = default;
};
