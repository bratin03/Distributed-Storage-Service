#ifndef DROPBOX_CONTENT_HASH_HPP
#define DROPBOX_CONTENT_HASH_HPP

#include <string>
#include <fstream>
#include <vector>
#include <stdexcept>

// Include PicoSHA2 – make sure picosha2.h is available in your project
#include "picosha2.h"

namespace dropbox {

    // Computes the Dropbox content hash for the given file.
    // The algorithm:
    //   1. Split the file into 4MB blocks.
    //   2. Compute the SHA-256 hash for each block.
    //   3. Concatenate all block hashes.
    //   4. Compute the SHA-256 hash of the concatenated hashes.
    inline std::string compute_content_hash(const std::string& file_path) {
        const std::size_t block_size = 4 * 1024 * 1024;  // 4 MB blocks
        std::ifstream file(file_path, std::ios::binary);
        if (!file) {
            throw std::runtime_error("Error opening file: " + file_path);
        }

        std::vector<unsigned char> buffer(block_size);
        std::vector<unsigned char> concatenated_hashes;

        while (file) {
            file.read(reinterpret_cast<char*>(buffer.data()), block_size);
            std::streamsize bytes_read = file.gcount();
            if (bytes_read <= 0) {
                break;
            }

            // Compute SHA-256 for the current block.
            std::vector<unsigned char> block_hash(picosha2::k_digest_size);
            picosha2::hash256(buffer.begin(), buffer.begin() + bytes_read, block_hash.begin(), block_hash.end());

            // Append the binary hash of the current block.
            concatenated_hashes.insert(concatenated_hashes.end(), block_hash.begin(), block_hash.end());
        }

        // Compute the overall SHA-256 hash on the concatenated block hashes.
        std::string overall_hash = picosha2::hash256_hex_string(concatenated_hashes);
        return overall_hash;
    }

} // namespace dropbox

#endif // DROPBOX_CONTENT_HASH_HPP
