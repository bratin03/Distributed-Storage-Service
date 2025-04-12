#include "deletion_manager.hpp"


DeletionManager DeletionManager::instance;

DeletionManager::DeletionManager() : stop_thread(false) {
    worker_thread = std::thread(&DeletionManager::process, this);
}

DeletionManager::~DeletionManager() {
    {
        std::unique_lock<std::mutex> lock(queue_mutex);
        stop_thread = true;
        cv.notify_all();
    }
    if (worker_thread.joinable()) {
        worker_thread.join();
    }
}

void DeletionManager::enqueue(const std::string &key) {
    {
        std::lock_guard<std::mutex> lock(queue_mutex);
        deletion_queue.push(key);
    }
    cv.notify_all();
}

void DeletionManager::process() {
    while (true) {
        std::unique_lock<std::mutex> lock(queue_mutex);
        cv.wait(lock, [&] { return !deletion_queue.empty() || stop_thread; });

        if (stop_thread && deletion_queue.empty()) break;

        std::string key = deletion_queue.front();
        deletion_queue.pop();
        lock.unlock();

        MyLogger::info("Deleting block data for key: " + key);
        auto res = Database_handler::delete_blockdata(key);
        if (!res.success) {
            MyLogger::warning("Failed to delete block data: " + res.err);
        }
    }
}
