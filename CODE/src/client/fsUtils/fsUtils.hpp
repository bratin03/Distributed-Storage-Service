/*
    CS60002 - Distributed Systems
    Term Project - Spring 2025

    * Author 1: Bratin Mondal (21CS10016)
    * Author 2: Soukhin Nayek (21CS10062)
    * Author 3: Swarnabh Mandal (21CS10068)

    * Department of Computer Science and Engineering
    * Indian Institute of Technology, Kharagpur
*/

#pragma once

#include <filesystem>
#include <string>

// filesystem -> fs
namespace fs = std::filesystem;

namespace fsUtils {

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

}  // namespace fsUtils
