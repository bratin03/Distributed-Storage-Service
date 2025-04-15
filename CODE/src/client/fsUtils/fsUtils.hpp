#pragma once

#include <string>
#include <filesystem>


// filesystem -> fs
namespace fs = std::filesystem;

namespace fsUtils
{

    extern std::string g_basePath;
    extern std::string g_user;
    void initialize(const std::string &basePath, const std::string &user);
    std::filesystem::path buildFullPath(const std::string &relativePath);
    void createTextFile(const std::string &relativePath, const std::string &content);
    std::string readTextFile(const std::string &relativePath);
    bool ensureDirectoryExists(const std::string &relativePath);
    void removeEntry(const std::string &relativePath);
    std::string computeSHA256Hash(const std::string &content);
    std::string buildKeyfromFullPath(fs::path fullPath);
    
}
