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
#include <condition_variable>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

class DeletionManager {
   public:
    static DeletionManager instance;

    void enqueue(const std::string &key);
    ~DeletionManager();

   private:
    DeletionManager();
    DeletionManager(const DeletionManager &) = delete;
    DeletionManager &operator=(const DeletionManager &) = delete;

    void process();

    std::vector<std::string> _deletionQueue;
    std::mutex _queueMutex;
    std::condition_variable _cv;
    std::thread _workerThread;
    bool _stopThread;
    const size_t _BATCH_SIZE_THRESHOLD;
    const unsigned int _MAX_WAIT_TIME;
};
