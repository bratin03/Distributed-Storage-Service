
// File: SimpleChunker.cpp
#include "Chunker.hpp"
#include <fstream>
#include <sstream>
#include <iomanip>
#include <openssl/sha.h>  // For SHA-256

std::string calculateSHA256(const std::vector<char>& data) {
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256(reinterpret_cast<const unsigned char*>(data.data()), data.size(), hash);
    std::stringstream ss;
    for (int i = 0; i < SHA256_DIGEST_LENGTH; i++) {
        ss << std::hex << std::setw(2) << std::setfill('0') << (int)hash[i];
    }
    return ss.str();
}

class SimpleChunker : public Chunker {
public:
    std::vector<Chunk> chunkFile(const std::string& filePath, size_t chunkSize) override {
        std::vector<Chunk> chunks;
        std::ifstream file(filePath, std::ios::binary);
        if (!file) return chunks;
        while (!file.eof()) {
            std::vector<char> buffer(chunkSize);
            file.read(buffer.data(), chunkSize);
            size_t bytesRead = file.gcount();
            if (bytesRead == 0) break;
            buffer.resize(bytesRead);
            Chunk c;
            c.data = buffer;
            c.id = calculateSHA256(buffer);
            chunks.push_back(c);
        }
        return chunks;
    }
    
    std::vector<char> reassembleFile(const std::vector<Chunk>& chunks) override {
        std::vector<char> fileData;
        for (const auto& chunk : chunks) {
            fileData.insert(fileData.end(), chunk.data.begin(), chunk.data.end());
        }
        return fileData;
    }
};
