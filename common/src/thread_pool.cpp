#include "thread_pool.h"

ThreadPool::ThreadPool(size_t thread_num) :stop_(false) {
    for(size_t i = 0; i < thread_num; ++i) {
        workers_.emplace_back(&ThreadPool::worker, this);
    }
}

ThreadPool::~ThreadPool(){
    {
    std::unique_lock<std::mutex> lock(mutex_);
    stop_ = true;
    }
    
    cond_var_.notify_all();
    for(auto& t : workers_){
        t.join();
    }
}

void ThreadPool::addTask(std::function<void()> task) {
    {
        std::unique_lock<std::mutex> lock(mutex_);
        tasks_.push(task);
    }
    cond_var_.notify_one();
}

void ThreadPool::worker() {
    while(true) {
        std::function<void()> task;
        {
            std::unique_lock<std::mutex> lock(mutex_);
            cond_var_.wait(lock, [this] {
                return stop_ || !tasks_.empty();
            });
            if(stop_ && tasks_.empty()) {
                return;
            }
            task = tasks_.front();
            tasks_.pop();
        }
        task();
    }
}