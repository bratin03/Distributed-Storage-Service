// File: Watcher.hpp
#pragma once
#include <string>
#include <functional>
#include <vector>

// A simplified interface for a file system watcher
class Watcher {
public:
    using Callback = std::function<void(const std::string& filePath)>;

    // Start watching a directory
    virtual void startWatching(const std::string& directory, Callback callback) = 0;

    virtual ~Watcher() = default;
};
