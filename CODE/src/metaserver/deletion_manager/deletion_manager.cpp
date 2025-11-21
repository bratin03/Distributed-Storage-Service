/*
    CS60002 - Distributed Systems
    Term Project - Spring 2025

    * Author 1: Bratin Mondal (21CS10016)
    * Author 2: Soukhin Nayek (21CS10062)
    * Author 3: Swarnabh Mandal (21CS10068)

    * Department of Computer Science and Engineering
    * Indian Institute of Technology, Kharagpur
*/

#include "./deletion_manager.hpp"

#include <chrono>

#include "../database_handler/database_handler.hpp"
#include "Mylogger.hpp"

DeletionManager DeletionManager::instance;

DeletionManager::DeletionManager()
    : _stopThread(false), _BATCH_SIZE_THRESHOLD(10), _MAX_WAIT_TIME(3000) {
    MyLogger::info("DeletionManager initialized starting worker thread");
    _workerThread = std::thread(&DeletionManager::process, this);
}

DeletionManager::~DeletionManager() {
    MyLogger::info("Stopping DeletionManager worker thread");

    {
        std::unique_lock<std::mutex> lock(_queueMutex);
        _stopThread = true;
        _cv.notify_all();
        MyLogger::info("Worker thread notified to stop");
    }
    if (_workerThread.joinable()) {
        MyLogger::info("Waiting for worker thread to finish");
        _workerThread.join();
        MyLogger::info("Worker thread finished");
    }
}

void DeletionManager::enqueue(const std::string &key) {
    {
        std::lock_guard<std::mutex> lock(_queueMutex);
        _deletionQueue.push_back(key);
        MyLogger::info("Enqueued key for deletion: " + key);
    }
    _cv.notify_all();
}

void DeletionManager::process() {
    while (true) {
        std::unique_lock<std::mutex> lock(_queueMutex);

        // Wait until there are enough items in the queue or timeout occurs
        _cv.wait_for(lock, std::chrono::milliseconds(_MAX_WAIT_TIME),
                    [&] { return _deletionQueue.size() >= _BATCH_SIZE_THRESHOLD || _stopThread; });

        if (_stopThread && _deletionQueue.empty()) break;

        // Move all the elements from the queue to a batch
        std::vector<std::string> batch = std::move(_deletionQueue);

        // Reinitialize the deletion queue to empty
        _deletionQueue.clear();

        lock.unlock();

        // Now we have a batch, so let's process it
        for (const auto &key : batch) {
            MyLogger::info("Deleting block data for key: " + key);
            auto res = Database_handler::delete_blockdata(key);
            if (!res.success) {
                MyLogger::warning("Failed to delete block data for key " + key +
                                  " | error: " + res.err);
            }
        }
    }
}
