#pragma once
#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <functional>

class ThreadPool {
public:
    ThreadPool(size_t thread_num = 4);
    ~ThreadPool();

    void addTask(std::function<void()> task);
private:
    void worker();

private:
    std::vector<std::thread> workers_;
    std::queue<std::function<void()>> tasks_;
    
    std::mutex mutex_;
    std::condition_variable cond_var_;
    bool stop_;
};