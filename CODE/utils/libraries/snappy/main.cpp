/*
    CS60002 - Distributed Systems
    Term Project - Spring 2025

    * Author 1: Bratin Mondal (21CS10016)
    * Author 2: Soukhin Nayek (21CS10062)
    * Author 3: Swarnabh Mandal (21CS10068)

    * Department of Computer Science and Engineering
    * Indian Institute of Technology, Kharagpur
*/
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <chrono>
#include "snappy.h"

int main() {
    // Open the file in binary mode.
    std::ifstream file("/tmp/a.txt", std::ios::in | std::ios::binary);
    if (!file) {
        std::cerr << "Error: Could not open /tmp/a.txt" << std::endl;
        return 1;
    }

    // Read the entire file into a string.
    std::ostringstream oss;
    oss << file.rdbuf();
    std::string input = oss.str();

    if (input.empty()) {
        std::cerr << "Error: File is empty." << std::endl;
        return 1;
    }

    // Variables to hold compressed and decompressed data.
    std::string compressed;
    std::string decompressed;

    // Compress the data and time the compression.
    auto startCompress = std::chrono::steady_clock::now();
    snappy::Compress(input.data(), input.size(), &compressed);
    auto endCompress = std::chrono::steady_clock::now();
    std::chrono::duration<double> compressDuration = endCompress - startCompress;

    // Calculate compression ratio: compressed size divided by original size.
    double ratio = static_cast<double>(compressed.size()) / input.size();

    // Decompress the data and time the decompression.
    auto startDecompress = std::chrono::steady_clock::now();
    bool success = snappy::Uncompress(compressed.data(), compressed.size(), &decompressed);
    auto endDecompress = std::chrono::steady_clock::now();
    std::chrono::duration<double> decompressDuration = endDecompress - startDecompress;

    if (!success) {
        std::cerr << "Error: Decompression failed!" << std::endl;
        return 1;
    }

    // Verify that decompressed data matches the original.
    if (input != decompressed) {
        std::cerr << "Error: Decompressed data does not match the original." << std::endl;
        return 1;
    }

    // Output the results.
    std::cout << "Original size:   " << input.size() << " bytes" << std::endl;
    std::cout << "Compressed size: " << compressed.size() << " bytes" << std::endl;
    std::cout << "Compression ratio: " << ratio 
              << " (" << ratio * 100 << "% of original size)" << std::endl;
    std::cout << "Time taken for compression:   " << compressDuration.count() << " seconds" << std::endl;
    std::cout << "Time taken for decompression: " << decompressDuration.count() << " seconds" << std::endl;

    return 0;
}
