#ifndef FSUTILS_HPP
#define FSUTILS_HPP

#include <string>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <openssl/sha.h>
#include <system_error>
#include "../logger/Mylogger.hpp"

namespace fsUtils
{

    /**
     * @brief Provides file system utility operations based on a pre-configured base path and user.
     *
     * This class encapsulates file and directory operations. It automatically handles path construction
     * using the base path and user. Only .txt file operations are permitted.
     */
    class FileSystemUtils
    {
    public:
        /**
         * @brief Constructor for FileSystemUtils.
         *
         * Initializes the base directory path and user identifier.
         * These values are used to construct full file system paths for all operations.
         *
         * @param basePath The root directory for file system operations.
         * @param user The user identifier used in relative paths (e.g., "username").
         */
        FileSystemUtils(const std::string &basePath, const std::string &user);

        //////////// File Operations ////////////

        /**
         * @brief Creates a text file with the given content.
         *
         * Checks if the file has a .txt extension. If not, logs an error and aborts.
         * Constructs the full path from the relative path and writes the provided content.
         *
         * @param relativePath The relative file path in the format "user:dropbox/yourfile.txt".
         * @param content The content to write into the file.
         */
        void createTextFile(const std::string &relativePath, const std::string &content);

        /**
         * @brief Reads the content of a text file.
         *
         * Checks if the file has a .txt extension. If not, logs an error and aborts.
         * Opens the file at the computed path and returns its complete content.
         *
         * @param relativePath The relative file path in the format "user:dropbox/yourfile.txt".
         * @return std::string The content of the text file, or an empty string on failure.
         */
        std::string readTextFile(const std::string &relativePath);

        //////////// Directory Operations ////////////

        /**
         * @brief Ensures that a directory exists.
         *
         * Constructs the full path from the relative path and creates the directory (with intermediate directories) if needed.
         * Logs the outcome whether the directory was created, already existed, or an error occurred.
         *
         * @param relativePath The relative directory path in the format "user:dropbox/yourdir".
         * @return true if the directory exists or was successfully created; false otherwise.
         */
        bool ensureDirectoryExists(const std::string &relativePath);

        /**
         * @brief Removes a file or directory entry.
         *
         * Constructs the full path from the relative path, then checks if the entry exists.
         * If the entry is a directory, it removes all its contents; if it is a file, it removes the file.
         * Logs the success or failure of the removal operation.
         *
         * @param relativePath The relative path of the file or directory to remove.
         */
        void removeEntry(const std::string &relativePath);

        //////////// Hash Operations ////////////

        /**
         * @brief Computes the SHA-256 hash of the given content.
         *
         * Uses OpenSSL to compute the SHA-256 hash and returns the hash as a hexadecimal string.
         * Useful for verifying file content integrity.
         *
         * @param content The string content for which the hash is computed.
         * @return std::string The computed SHA-256 hash in hexadecimal format.
         */
        std::string computeSHA256Hash(const std::string &content);

    private:
        std::string m_basePath;
        std::string m_user;

        /**
         * @brief Builds the full filesystem path from a relative path.
         *
         * Removes the user-specific prefix (e.g., "user:dropbox/") from the given relative path and
         * appends the remaining part to the base path.
         *
         * @param relativePath The relative path to process.
         * @return std::filesystem::path The constructed full filesystem path.
         */
        std::filesystem::path buildFullPath(const std::string &relativePath);
    };

} // namespace fsUtils

#endif // FSUTILS_HPP
