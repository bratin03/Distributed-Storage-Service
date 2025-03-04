#include <zlib.h>
#include <openssl/md5.h>
#include <vector>
#include <string>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <stdexcept>

// Define a type alias for our checksum pair:
// first: Adler-32 (uint32_t), second: MD5 hash as a hex string.
using ChecksumPair = std::pair<uint32_t, std::string>;

/**
 * @brief Computes checksums for each 4MB block of the given file.
 *
 * @param filename Path to the file.
 * @return std::vector<ChecksumPair> A vector of checksum pairs for consecutive 4MB chunks.
 * @throws std::runtime_error if the file cannot be opened.
 */
std::vector<ChecksumPair> computeChecksums(const std::string &filename)
{
    const size_t CHUNK_SIZE = 4 * 1024 * 1024; // 4MB
    std::vector<ChecksumPair> results;

    std::ifstream file(filename, std::ios::binary);
    if (!file)
    {
        throw std::runtime_error("Could not open file: " + filename);
    }

    // Allocate a buffer to hold each chunk.
    std::vector<char> buffer(CHUNK_SIZE);

    while (file)
    {
        // Read up to CHUNK_SIZE bytes
        file.read(buffer.data(), CHUNK_SIZE);
        std::streamsize bytesRead = file.gcount();
        if (bytesRead <= 0)
            break;

        // --- Compute Adler-32 checksum using zlib ---
        // Initialize with 0L and then update with the block data.
        uint32_t weakChecksum = adler32(0L, Z_NULL, 0);
        weakChecksum = adler32(weakChecksum, reinterpret_cast<const Bytef *>(buffer.data()), bytesRead);

        // --- Compute MD5 checksum using OpenSSL ---
        unsigned char md5Result[MD5_DIGEST_LENGTH];
        MD5(reinterpret_cast<const unsigned char *>(buffer.data()), bytesRead, md5Result);

        // Convert the MD5 digest to a hexadecimal string.
        std::ostringstream oss;
        oss << std::hex << std::setfill('0');
        for (int i = 0; i < MD5_DIGEST_LENGTH; ++i)
        {
            oss << std::setw(2) << static_cast<int>(md5Result[i]);
        }
        std::string strongChecksum = oss.str();

        // Store the pair in the results vector.
        results.emplace_back(weakChecksum, strongChecksum);
    }

    return results;
}
