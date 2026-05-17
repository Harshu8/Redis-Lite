#pragma once

#include <condition_variable>
#include <functional>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

namespace redis_lite {

class ThreadPool {
public:
    using JobHandler = std::function<void(int)>;

    ThreadPool(std::size_t threadCount, JobHandler jobHandler);
    ~ThreadPool();

    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;

    void addClient(int clientFd);

private:
    void workerLoop();

    std::vector<std::thread> workers_;
    std::queue<int> clientQueue_;
    std::mutex queueMutex_;
    std::condition_variable condition_;
    bool stop_{false};
    JobHandler jobHandler_;
};

} // namespace redis_lite
