#include <fstream>
#include <iostream>
#include <string>

// Dummy Logger for demonstration
namespace Logger
{
    void info(const std::string &msg)
    {
        std::cout << "[INFO] " << msg << std::endl;
    }
    void error(const std::string &msg)
    {
        std::cerr << "[ERROR] " << msg << std::endl;
    }
}

// Dummy buildPath function that simply appends the relative path to base path
std::string buildPath(const std::string &basePath, const std::string &relativePath)
{
    return basePath + "/" + relativePath;
}

// Provided createFile function
inline void createFile(const std::string &relativePath, const std::string &basePath, const std::string &content)
{
    auto filePath = buildPath(basePath, relativePath);
    // Note: std::ios::binary is used, but typically you would also include std::ios::out.
    std::ofstream file(filePath, std::ios::binary);
    if (file.is_open())
    {
        file << content;
        file.close();
        Logger::info("Created file: " + filePath);
    }
    else
    {
        Logger::error("Error creating file '" + filePath + "'");
    }
}

int main()
{
    std::string basePath = "./"; // Current directory
    std::string relativePath = "example.txt";

    // First call: creates the file with "First content"
    createFile(relativePath, basePath, "First content");

    // Second call: the file "example.txt" already exists and will be overwritten with "Second content"
    createFile(relativePath, basePath, "Second content");

    return 0;
}
