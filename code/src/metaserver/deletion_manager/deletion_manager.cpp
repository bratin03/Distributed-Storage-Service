#include "./deletion_manager.hpp"
#include "../database_handler/database_handler.hpp"
#include "../logger/Mylogger.h"
#include <chrono>

DeletionManager DeletionManager::instance;

DeletionManager::DeletionManager() : stop_thread(false), BATCH_SIZE_THRESHOLD(10),
                                     MAX_WAIT_TIME(3000)
{
    MyLogger::info("DeletionManager initialized starting worker thread");
    worker_thread = std::thread(&DeletionManager::process, this);
}

DeletionManager::~DeletionManager()
{

    MyLogger::info("Stopping DeletionManager worker thread");

    {
        std::unique_lock<std::mutex> lock(queue_mutex);
        stop_thread = true;
        cv.notify_all();
        MyLogger::info("Worker thread notified to stop");
    }
    if (worker_thread.joinable())
    {
        MyLogger::info("Waiting for worker thread to finish");
        worker_thread.join();
        MyLogger::info("Worker thread finished");
    }
}

void DeletionManager::enqueue(const std::string &key)
{
    {
        std::lock_guard<std::mutex> lock(queue_mutex);
        deletion_queue.push_back(key);
        MyLogger::info("Enqueued key for deletion: " + key);
    }
    cv.notify_all();
}

void DeletionManager::process()
{
    while (true)
    {
        std::unique_lock<std::mutex> lock(queue_mutex);

        // Wait until there are enough items in the queue or timeout occurs
        cv.wait_for(lock, std::chrono::milliseconds(MAX_WAIT_TIME), [&]
                    { return deletion_queue.size() >= BATCH_SIZE_THRESHOLD || stop_thread; });

        if (stop_thread && deletion_queue.empty())
            break;

        // Move all the elements from the queue to a batch
        std::vector<std::string> batch = std::move(deletion_queue);

        // Reinitialize the deletion queue to empty
        deletion_queue.clear();

        lock.unlock();

        // Now we have a batch, so let's process it
        for (const auto &key : batch)
        {
            MyLogger::info("Deleting block data for key: " + key);
            auto res = Database_handler::delete_blockdata(key);
            if (!res.success)
            {
                MyLogger::warning("Failed to delete block data for key " + key + " | error: " + res.err);
            }
        }
    }
}
