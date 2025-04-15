#ifndef DROPBOX_CONTENT_HASH_HPP
#define DROPBOX_CONTENT_HASH_HPP

#include <string>
#include <vector>
#include <algorithm> // for std::min

// Include PicoSHA2 – make sure picosha2.h is available in your project
#include "picosha2.h"

namespace dropbox
{

    // Computes the Dropbox content hash for the given file content.
    // The algorithm:
    //   1. Split the content into 4MB blocks.
    //   2. Compute the SHA-256 hash for each block.
    //   3. Concatenate all block hashes.
    //   4. Compute the SHA-256 hash of the concatenated hashes.
    inline std::string compute_content_hash(const std::string &content)
    {
        const std::size_t block_size = 4 * 1024 * 1024; // 4 MB blocks
        std::vector<unsigned char> concatenated_hashes;

        for (std::size_t pos = 0; pos < content.size(); pos += block_size)
        {
            std::size_t current_block_size = std::min(block_size, content.size() - pos);
            std::vector<unsigned char> block_hash(picosha2::k_digest_size);

            // Compute SHA-256 for the current block.
            // Using content iterators which are valid due to C++11 contiguous storage guarantee.
            picosha2::hash256(content.begin() + pos, content.begin() + pos + current_block_size,
                                block_hash.begin(), block_hash.end());

            // Append the binary hash of the current block.
            concatenated_hashes.insert(concatenated_hashes.end(),
                                       block_hash.begin(), block_hash.end());
        }

        // Compute the overall SHA-256 hash on the concatenated block hashes.
        std::string overall_hash = picosha2::hash256_hex_string(concatenated_hashes);
        return overall_hash;
    }

} // namespace dropbox

#endif // DROPBOX_CONTENT_HASH_HPP
