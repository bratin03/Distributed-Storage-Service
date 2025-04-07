#include "fsUtils.hpp"

namespace fsUtils
{

    FileSystemUtils::FileSystemUtils(const std::string &basePath, const std::string &user)
        : m_basePath(basePath), m_user(user)
    {
        MyLogger::info("Initialized FileSystemUtils with base path: " + m_basePath + " and user: " + m_user);
    }

    std::filesystem::path FileSystemUtils::buildFullPath(const std::string &relativePath)
    {
        // Expecting the relative path in the format: "user:dropbox/..."
        std::string prefix = m_user + ":dropbox/";
        std::string pathStr = relativePath;
        if (relativePath.rfind(prefix, 0) == 0)
        { // If the relative path starts with the prefix
            pathStr = relativePath.substr(prefix.length());
        }
        else
        {
            MyLogger::error("Relative path does not start with expected prefix (" + prefix + "): " + relativePath);
        }
        auto fullPath = std::filesystem::path(m_basePath) / pathStr;
        MyLogger::info("Constructed full path: " + fullPath.string());
        return fullPath;
    }

    void FileSystemUtils::createTextFile(const std::string &relativePath, const std::string &content)
    {
        // Only allow .txt files
        if (std::filesystem::path(relativePath).extension() != ".txt")
        {
            MyLogger::error("File creation aborted: Only .txt files are allowed (" + relativePath + ")");
            return;
        }
        auto filePath = buildFullPath(relativePath);
        std::ofstream file(filePath, std::ios::binary);
        if (file.is_open())
        {
            file << content;
            file.close();
            MyLogger::info("Successfully created text file: " + filePath.string());
        }
        else
        {
            MyLogger::error("Error creating text file: " + filePath.string());
        }
    }

    std::string FileSystemUtils::readTextFile(const std::string &relativePath)
    {
        // Only allow .txt files
        if (std::filesystem::path(relativePath).extension() != ".txt")
        {
            MyLogger::error("File read aborted: Only .txt files are allowed (" + relativePath + ")");
            return "";
        }
        auto filePath = buildFullPath(relativePath);
        std::ifstream file(filePath);
        if (file.is_open())
        {
            std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
            file.close();
            MyLogger::info("Successfully read text file: " + filePath.string());
            return content;
        }
        else
        {
            MyLogger::error("Error reading text file: " + filePath.string());
            return "";
        }
    }

    bool FileSystemUtils::ensureDirectoryExists(const std::string &relativePath)
    {
        auto dirPath = buildFullPath(relativePath);
        std::error_code ec;
        if (!std::filesystem::exists(dirPath, ec))
        {
            bool created = std::filesystem::create_directories(dirPath, ec);
            if (ec)
            {
                MyLogger::error("Error creating directory '" + dirPath.string() + "': " + ec.message());
                return false;
            }
            if (created)
            {
                MyLogger::info("Successfully created directory: " + dirPath.string());
            }
            else
            {
                MyLogger::info("Directory already existed: " + dirPath.string());
            }
        }
        else
        {
            MyLogger::info("Directory verified as existing: " + dirPath.string());
        }
        return true;
    }

    void FileSystemUtils::removeEntry(const std::string &relativePath)
    {
        auto entryPath = buildFullPath(relativePath);
        std::error_code ec;
        if (!std::filesystem::exists(entryPath, ec))
        {
            MyLogger::error("Removal aborted: Entry does not exist: " + entryPath.string());
            return;
        }
        if (std::filesystem::is_directory(entryPath, ec))
        {
            auto count = std::filesystem::remove_all(entryPath, ec);
            if (ec)
                MyLogger::error("Failed to remove directory '" + entryPath.string() + "': " + ec.message());
            else
                MyLogger::info("Removed directory (" + std::to_string(count) + " entries): " + entryPath.string());
        }
        else
        {
            bool removed = std::filesystem::remove(entryPath, ec);
            if (!removed || ec)
                MyLogger::error("Failed to remove file '" + entryPath.string() + "': " + ec.message());
            else
                MyLogger::info("Removed file: " + entryPath.string());
        }
    }

    std::string FileSystemUtils::computeSHA256Hash(const std::string &content)
    {
        unsigned char hash[SHA256_DIGEST_LENGTH];
        SHA256(reinterpret_cast<const unsigned char *>(content.c_str()), content.size(), hash);
        std::ostringstream oss;
        for (unsigned char byte : hash)
        {
            oss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(byte);
        }
        MyLogger::info("Computed SHA256 hash for provided content.");
        return oss.str();
    }

} // namespace fsUtils
