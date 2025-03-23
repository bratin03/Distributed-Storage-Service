
// File: SimpleChunker.cpp
#include "Chunker.hpp"
#include <fstream>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <openssl/sha.h>  // For SHA-256
#include <openssl/evp.h>  // For hash functions
#include <filesystem>
namespace fs = std::filesystem;

// Chunker implementation
std::vector<ChunkMetadata> Chunker::splitFile(const std::string& file_path, const std::string& output_dir) {
    std::vector<ChunkMetadata> chunks;
    
    std::ifstream file(file_path, std::ios::binary);
    if (!file) {
        std::cerr << "Failed to open file: " << file_path << std::endl;
        return chunks;
    }
    
    // Create output directory if it doesn't exist
    fs::create_directories(output_dir);
    
    // Get file size
    file.seekg(0, std::ios::end);
    size_t file_size = file.tellg();
    file.seekg(0, std::ios::beg);
    
    // Calculate number of chunks
    size_t num_chunks = (file_size + CHUNK_SIZE - 1) / CHUNK_SIZE;
    
    char* buffer = new char[CHUNK_SIZE];
    
    for (size_t i = 0; i < num_chunks; i++) {
        size_t offset = i * CHUNK_SIZE;
        size_t size = std::min(CHUNK_SIZE, file_size - offset);
        
        file.seekg(offset, std::ios::beg);
        file.read(buffer, size);
        
        // Calculate hash for the chunk
        std::string chunk_hash = calculateChunkHash(buffer, size);
        
        // Create chunk metadata
        ChunkMetadata chunk;
        chunk.chunk_id = chunk_hash;
        chunk.offset = i;
        chunk.size = size;
        
        chunks.push_back(chunk);
        
        // Write chunk to file
        std::string chunk_path = output_dir + "/" + chunk_hash;
        std::ofstream chunk_file(chunk_path, std::ios::binary);
        if (!chunk_file) {
            std::cerr << "Failed to create chunk file: " << chunk_path << std::endl;
            continue;
        }
        
        chunk_file.write(buffer, size);
        chunk_file.close();
    }
    
    delete[] buffer;
    file.close();
    
    return chunks;
}

void Chunker::reassembleFile(const std::string& output_path, const std::vector<ChunkMetadata>& chunks, const std::string& chunks_dir) {
    // Create parent directories if they don't exist
    fs::create_directories(fs::path(output_path).parent_path());
    
    std::ofstream output_file(output_path, std::ios::binary | std::ios::trunc);
    if (!output_file) {
        std::cerr << "Failed to create output file: " << output_path << std::endl;
        return;
    }
    
    for (const auto& chunk : chunks) {
        std::string chunk_path = chunks_dir + "/" + chunk.chunk_id;
        std::ifstream chunk_file(chunk_path, std::ios::binary);
        
        if (!chunk_file) {
            std::cerr << "Failed to open chunk file: " << chunk_path << std::endl;
            continue;
        }
        
        char* buffer = new char[chunk.size];
        chunk_file.read(buffer, chunk.size);
        
        // Verify chunk integrity
        std::string chunk_hash = calculateChunkHash(buffer, chunk.size);
        if (chunk_hash != chunk.chunk_id) {
            std::cerr << "Chunk integrity check failed for: " << chunk_path << std::endl;
            delete[] buffer;
            continue;
        }
        
        // Write chunk to the output file
        output_file.seekp(chunk.offset * CHUNK_SIZE, std::ios::beg);
        output_file.write(buffer, chunk.size);
        
        delete[] buffer;
        chunk_file.close();
    }
    
    output_file.close();
}

std::string Chunker::calculateChunkHash(const char* data, size_t size) {
    unsigned char hash[SHA256_DIGEST_LENGTH];
    EVP_MD_CTX* ctx = EVP_MD_CTX_new(); // Create a new context
    if (!ctx) {
        throw std::runtime_error("Failed to create EVP_MD_CTX");
    }

    if (EVP_DigestInit_ex(ctx, EVP_sha256(), nullptr) != 1 ||
        EVP_DigestUpdate(ctx, data, size) != 1 ||
        EVP_DigestFinal_ex(ctx, hash, nullptr) != 1) {
        EVP_MD_CTX_free(ctx);
        throw std::runtime_error("Failed to compute SHA-256 hash");
    }

    EVP_MD_CTX_free(ctx); // Free the context

    std::stringstream ss;
    for (int i = 0; i < SHA256_DIGEST_LENGTH; i++) {
        ss << std::hex << std::setw(2) << std::setfill('0') << (int)hash[i];
    }

    return ss.str();
}

std::string Chunker::calculateFileHash(const std::vector<ChunkMetadata>& chunks) {
    unsigned char hash[SHA256_DIGEST_LENGTH];
    EVP_MD_CTX* ctx = EVP_MD_CTX_new(); // Create a new context
    if (!ctx) {
        throw std::runtime_error("Failed to create EVP_MD_CTX");
    }

    if (EVP_DigestInit_ex(ctx, EVP_sha256(), nullptr) != 1) {
        EVP_MD_CTX_free(ctx);
        throw std::runtime_error("Failed to initialize SHA-256 context");
    }

    for (const auto& chunk : chunks) {
        if (EVP_DigestUpdate(ctx, chunk.chunk_id.c_str(), chunk.chunk_id.size()) != 1) {
            EVP_MD_CTX_free(ctx);
            throw std::runtime_error("Failed to update SHA-256 hash");
        }
    }

    if (EVP_DigestFinal_ex(ctx, hash, nullptr) != 1) {
        EVP_MD_CTX_free(ctx);
        throw std::runtime_error("Failed to finalize SHA-256 hash");
    }

    EVP_MD_CTX_free(ctx); // Free the context

    std::stringstream ss;
    for (int i = 0; i < SHA256_DIGEST_LENGTH; i++) {
        ss << std::hex << std::setw(2) << std::setfill('0') << (int)hash[i];
    }

    return ss.str();
}
