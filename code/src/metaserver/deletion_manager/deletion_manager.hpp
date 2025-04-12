#pragma once
#include <string>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>

class DeletionManager {
public:
    static DeletionManager instance;

    void enqueue(const std::string &key);
    ~DeletionManager();

private:
    DeletionManager();
    DeletionManager(const DeletionManager&) = delete;
    DeletionManager& operator=(const DeletionManager&) = delete;

    void process();

    std::queue<std::string> deletion_queue;
    std::mutex queue_mutex;
    std::condition_variable cv;
    std::thread worker_thread;
    bool stop_thread;
};
