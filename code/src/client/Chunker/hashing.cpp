#include <iostream>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <unordered_map>
#include <vector>
#include <openssl/evp.h>

namespace fs = std::filesystem;

// Compute SHA256 hash for a given file using the EVP API.
std::string computeChunkHash(const std::string &filePath) {
    std::ifstream file(filePath, std::ios::binary);
    if (!file) {
        std::cerr << "Error opening file: " << filePath << std::endl;
        return "";
    }
    
    EVP_MD_CTX *mdctx = EVP_MD_CTX_new();
    if (!mdctx) {
        std::cerr << "Error creating EVP_MD_CTX" << std::endl;
        return "";
    }
    
    if (EVP_DigestInit_ex(mdctx, EVP_sha256(), nullptr) != 1) {
        std::cerr << "Error initializing digest" << std::endl;
        EVP_MD_CTX_free(mdctx);
        return "";
    }
    
    const size_t bufferSize = 4096;
    char buffer[bufferSize];
    while (file.good()) {
        file.read(buffer, bufferSize);
        std::streamsize bytesRead = file.gcount();
        if (bytesRead > 0) {
            if (EVP_DigestUpdate(mdctx, buffer, bytesRead) != 1) {
                std::cerr << "Error updating digest" << std::endl;
                EVP_MD_CTX_free(mdctx);
                return "";
            }
        }
    }
    
    unsigned char md[EVP_MAX_MD_SIZE];
    unsigned int md_len = 0;
    if (EVP_DigestFinal_ex(mdctx, md, &md_len) != 1) {
        std::cerr << "Error finalizing digest" << std::endl;
        EVP_MD_CTX_free(mdctx);
        return "";
    }
    
    EVP_MD_CTX_free(mdctx);
    
    std::stringstream ss;
    for (unsigned int i = 0; i < md_len; ++i) {
        ss << std::hex << std::setw(2) << std::setfill('0') << (int)md[i];
    }
    
    return ss.str();
}

// Load all chunk files from a folder, computing their hashes.
// Returns a mapping: hash value -> vector of chunk filenames (only the filename, not the full path).
std::unordered_map<std::string, std::vector<std::string>> loadFolderHashes(const std::string &folderName) {
    std::unordered_map<std::string, std::vector<std::string>> hashMap;
    
    for (auto &entry : fs::directory_iterator(folderName)) {
        if (entry.is_regular_file()) {
            std::string path = entry.path().string();
            std::string hash = computeChunkHash(path);
            if (!hash.empty()) {
                hashMap[hash].push_back(entry.path().filename().string());
            }
        }
    }
    
    return hashMap;
}

int main(int argc, char* argv[]) {
    if (argc < 3) {
        std::cout << "Usage: " << argv[0] << " <folder1> <folder2>\n";
        std::cout << "  folder1: original chunks\n";
        std::cout << "  folder2: modified chunks\n";
        return 1;
    }
    
    std::string folder1 = argv[1];
    std::string folder2 = argv[2];
    
    auto hashMap1 = loadFolderHashes(folder1);
    auto hashMap2 = loadFolderHashes(folder2);
    
    std::vector<std::string> toDelete;
    std::vector<std::string> toAdd;
    
    // Compare folder1 against folder2:
    // For each hash in folder1, if it is not in folder2 or if folder1 has extra occurrences, mark those chunk files for deletion.
    for (const auto &pair : hashMap1) {
        const std::string &hash = pair.first;
        const std::vector<std::string> &files1 = pair.second;
        auto it = hashMap2.find(hash);
        if (it == hashMap2.end()) {
            // This hash is not present in folder2, so all corresponding chunks in folder1 are obsolete.
            for (const auto &filename : files1) {
                toDelete.push_back(folder1 + "/" + filename);
            }
        } else {
            const std::vector<std::string> &files2 = it->second;
            if (files1.size() > files2.size()) {
                size_t diff = files1.size() - files2.size();
                for (size_t i = 0; i < diff; ++i) {
                    toDelete.push_back(folder1 + "/" + files1[i]);
                }
            }
        }
    }
    
    // Compare folder2 against folder1:
    // For each hash in folder2, if it is not in folder1 or if folder2 has extra occurrences, mark those chunk files for addition.
    for (const auto &pair : hashMap2) {
        const std::string &hash = pair.first;
        const std::vector<std::string> &files2 = pair.second;
        auto it = hashMap1.find(hash);
        if (it == hashMap1.end()) {
            // This hash is new in folder2.
            for (const auto &filename : files2) {
                toAdd.push_back(folder2 + "/" + filename);
            }
        } else {
            const std::vector<std::string> &files1 = it->second;
            if (files2.size() > files1.size()) {
                size_t diff = files2.size() - files1.size();
                for (size_t i = 0; i < diff; ++i) {
                    toAdd.push_back(folder2 + "/" + files2[i]);
                }
            }
        }
    }
    
    // Output the results.
    std::cout << "Chunks to DELETE (present in " << folder1 << " but not in " << folder2 << "):\n";
    for (const auto &file : toDelete) {
        std::cout << "  " << file << "\n";
    }
    
    std::cout << "\nChunks to ADD (present in " << folder2 << " but not in " << folder1 << "):\n";
    for (const auto &file : toAdd) {
        std::cout << "  " << file << "\n";
    }
    
    return 0;
}
