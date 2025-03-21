
// File: main.cpp
#include <iostream>
#include "Watcher.hpp"
#ifdef __linux__
#include "InotifyWatcher.cpp"
#endif
#include "SimpleChunker.cpp"
#include "SimpleIndexer.cpp"

int main() {
    // Choose a directory to watch (adjust as needed)
    std::string watchDir = "/path/to/sync/folder";
    
#ifdef __linux__
    InotifyWatcher watcher;
#else
    // On other platforms, use a different watcher implementation.
    // For this example, we assume Linux.
    InotifyWatcher watcher;
#endif

    SimpleChunker chunker;
    SimpleIndexer indexer;

    // Start watching for file changes
    watcher.startWatching(watchDir, [&](const std::string& fileName) {
        std::string fullPath = watchDir + "/" + fileName;
        std::cout << "Change detected in file: " << fullPath << std::endl;
        // Chunk the changed file
        auto chunks = chunker.chunkFile(fullPath);
        // Update the index with new chunk info
        indexer.updateFileIndex(fullPath, chunks);
        // Next step: trigger upload of changed chunks to the server.
    });
    
    // Keep the main thread alive (in a real application, use proper event loop)
    while (true) { std::this_thread::sleep_for(std::chrono::seconds(10)); }
    
    return 0;
}
