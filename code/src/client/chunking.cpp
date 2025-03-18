#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <deque>
#include <string>
#include <cstdlib>
#include <filesystem>
#include <vector>
#include <algorithm>

namespace fs = std::filesystem;

// Configuration parameters.
const int MIN_CHUNK_SIZE = 1024;       // 1KB minimum chunk size.
const int MAX_CHUNK_SIZE = 8192;       // 8KB maximum chunk size.
const int WINDOW_SIZE = 48;            // Size of the sliding window for the rolling hash.
const unsigned int CHUNK_MASK = 0xFFF; // Boundary condition: lower 12 bits zero.
const int BASE = 257;                  // Base value for the rolling hash.

// Compute the initial hash for a window stored in a deque.
unsigned int computeHash(const std::deque<unsigned char> &window)
{
    unsigned int hash = 0;
    for (unsigned char byte : window)
    {
        hash = hash * BASE + byte;
    }
    return hash;
}

// Update the rolling hash by removing the oldest byte and adding the new one.
unsigned int updateHash(unsigned int hash, unsigned char oldByte, unsigned char newByte, unsigned int basePower)
{
    hash = (hash - oldByte * basePower) * BASE + newByte;
    return hash;
}

// Splits the input file into chunks and saves them in the provided folder.
void fileToChunks(const std::string &filename, const std::string &folderName)
{
    std::ifstream inputFile(filename, std::ios::binary);
    if (!inputFile)
    {
        std::cerr << "Error opening input file: " << filename << std::endl;
        std::exit(EXIT_FAILURE);
    }

    // Create the folder if it does not exist.
    if (!fs::exists(folderName))
    {
        fs::create_directory(folderName);
    }

    // Precompute BASE^(WINDOW_SIZE-1) for hash updates.
    unsigned int basePower = 1;
    for (int i = 0; i < WINDOW_SIZE - 1; ++i)
    {
        basePower *= BASE;
    }

    int chunkNumber = 0;
    int chunkLength = 0;
    unsigned int hash = 0;
    std::deque<unsigned char> window;

    // Prepare the first chunk file inside the folder.
    std::ostringstream oss;
    oss << folderName << "/chunk_" << std::setw(3) << std::setfill('0') << chunkNumber << ".chunk";
    std::ofstream chunkFile(oss.str(), std::ios::binary);
    if (!chunkFile)
    {
        std::cerr << "Error opening chunk file: " << oss.str() << std::endl;
        std::exit(EXIT_FAILURE);
    }

    char ch;
    while (inputFile.get(ch))
    {
        unsigned char byte = static_cast<unsigned char>(ch);
        chunkFile.put(ch);
        ++chunkLength;

        // Update the rolling window.
        if (window.size() < WINDOW_SIZE)
        {
            window.push_back(byte);
            if (window.size() == WINDOW_SIZE)
            {
                hash = computeHash(window);
            }
        }
        else
        {
            hash = updateHash(hash, window.front(), byte, basePower);
            window.pop_front();
            window.push_back(byte);
        }

        // After reaching the minimum chunk size, check for the boundary condition.
        if (chunkLength >= MIN_CHUNK_SIZE)
        {
            if ((hash & CHUNK_MASK) == 0 || chunkLength >= MAX_CHUNK_SIZE)
            {
                chunkFile.close();
                std::cout << "Created chunk " << std::setw(3) << std::setfill('0') << chunkNumber
                          << " (" << chunkLength << " bytes) in folder '" << folderName << "'.\n";
                ++chunkNumber;
                chunkLength = 0;
                window.clear();
                oss.str("");
                oss.clear();
                oss << folderName << "/chunk_" << std::setw(3) << std::setfill('0') << chunkNumber << ".chunk";
                chunkFile.open(oss.str(), std::ios::binary);
                if (!chunkFile)
                {
                    std::cerr << "Error opening new chunk file: " << oss.str() << std::endl;
                    std::exit(EXIT_FAILURE);
                }
            }
        }
    }

    chunkFile.close();
    inputFile.close();
    std::cout << "File split into " << (chunkNumber + 1) << " chunks in folder '" << folderName << "'.\n";
}

// Automatically finds all chunk files in folderName that match the pattern "chunk_*.chunk",
// sorts them by filename, and merges them into outputFilename.
void chunksToFile(const std::string &outputFilename, const std::string &folderName)
{
    std::ofstream outputFile(outputFilename, std::ios::binary);
    if (!outputFile)
    {
        std::cerr << "Error opening output file: " << outputFilename << std::endl;
        std::exit(EXIT_FAILURE);
    }

    // Collect chunk files in the folder.
    std::vector<fs::directory_entry> chunkFiles;
    for (const auto &entry : fs::directory_iterator(folderName))
    {
        if (entry.is_regular_file())
        {
            std::string filename = entry.path().filename().string();
            // Check that the file name starts with "chunk_" and ends with ".chunk"
            if (filename.rfind("chunk_", 0) == 0 && filename.find(".chunk") != std::string::npos)
            {
                chunkFiles.push_back(entry);
            }
        }
    }

    if (chunkFiles.empty())
    {
        std::cerr << "No chunk files found in folder: " << folderName << std::endl;
        std::exit(EXIT_FAILURE);
    }

    // Sort the chunk files by filename.
    std::sort(chunkFiles.begin(), chunkFiles.end(), [](const fs::directory_entry &a, const fs::directory_entry &b)
              { return a.path().filename().string() < b.path().filename().string(); });

    // Merge each chunk file into the output file.
    for (const auto &entry : chunkFiles)
    {
        std::ifstream chunkFile(entry.path(), std::ios::binary);
        if (!chunkFile)
        {
            std::cerr << "Error opening chunk file: " << entry.path() << std::endl;
            std::exit(EXIT_FAILURE);
        }
        outputFile << chunkFile.rdbuf();
        chunkFile.close();
    }

    outputFile.close();
    std::cout << "Chunks merged into file: " << outputFilename << std::endl;
}

// Main function demonstrating both splitting and merging operations.
// Usage:
//   To split a file:   ./program split <input_file> <output_folder>
//   To merge chunks:   ./program merge <output_file> <chunk_folder> <num_chunks>
int main(int argc, char *argv[])
{
    if (argc < 3)
    {
        std::cout << "Usage:\n"
                  << "  To split: " << argv[0] << " split <input_file> <output_folder>\n"
                  << "  To merge: " << argv[0] << " merge <output_file> <chunk_folder>\n";
        return EXIT_FAILURE;
    }

    std::string command = argv[1];
    if (command == "split")
    {
        if (argc < 4)
        {
            std::cerr << "Usage: " << argv[0] << " split <input_file> <output_folder>\n";
            return EXIT_FAILURE;
        }
        fileToChunks(argv[2], argv[3]);
    }
    else if (command == "merge")
    {
        if (argc < 4)
        {
            std::cerr << "Usage: " << argv[0] << " merge <output_file> <chunk_folder>\n";
            return EXIT_FAILURE;
        }
        chunksToFile(argv[2], argv[3]);
    }
    else
    {
        std::cerr << "Unknown command: " << command << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
