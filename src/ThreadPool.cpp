#include "ThreadPool.hpp"

#include <utility>

namespace redis_lite {

ThreadPool::ThreadPool(std::size_t threadCount, JobHandler jobHandler)
    : jobHandler_(std::move(jobHandler)) {
    for (std::size_t i = 0; i < threadCount; ++i) {
        workers_.emplace_back([this]() {
            workerLoop();
        });
    }
}

ThreadPool::~ThreadPool() {
    {
        std::lock_guard<std::mutex> lock(queueMutex_);
        stop_ = true;
    }

    condition_.notify_all();

    for (std::thread& worker : workers_) {
        if (worker.joinable()) {
            worker.join();
        }
    }
}

void ThreadPool::addClient(int clientFd) {
    {
        std::lock_guard<std::mutex> lock(queueMutex_);
        clientQueue_.push(clientFd);
    }

    condition_.notify_one();
}

void ThreadPool::workerLoop() {
    while (true) {
        int clientFd = -1;

        {
            std::unique_lock<std::mutex> lock(queueMutex_);

            condition_.wait(lock, [this]() {
                return stop_ || !clientQueue_.empty();
            });

            if (stop_ && clientQueue_.empty()) {
                return;
            }

            clientFd = clientQueue_.front();
            clientQueue_.pop();
        }

        jobHandler_(clientFd);
    }
}

} // namespace redis_lite
