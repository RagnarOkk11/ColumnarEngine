#pragma once

#include "utils/Concurrency.h"
#include <vector>
#include <functional>

#ifdef ENABLE_MULTITHREADING

#include <thread>
#include <condition_variable>

class ThreadPool {
public:
    explicit ThreadPool(size_t num_threads) : stop_(false) {
        for (size_t i = 0; i < num_threads; ++i) {
            workers_.emplace_back([this] {
                for (;;) {
                    std::function<void()> task;

                    {
                        std::unique_lock<std::mutex> lock(this->queue_mutex_);
                        this->condition_.wait(
                            lock, [this] { return this->stop_ || !this->tasks_.empty(); });

                        if (this->stop_ && this->tasks_.empty()) {
                            return;
                        }

                        task = std::move(this->tasks_.back());
                        this->tasks_.pop_back();
                    }

                    task();
                }
            });
        }
    }

    void Enqueue(std::function<void()> task) {
        {
            std::unique_lock<std::mutex> lock(queue_mutex_);
            tasks_.push_back(std::move(task));
        }
        condition_.notify_one();
    }

    ~ThreadPool() {
        {
            std::unique_lock<std::mutex> lock(queue_mutex_);
            stop_ = true;
        }
        condition_.notify_all();
        for (std::thread& worker : workers_) {
            worker.join();
        }
    }

private:
    std::vector<std::thread> workers_;
    std::vector<std::function<void()>> tasks_;
    std::mutex queue_mutex_;
    std::condition_variable condition_;
    bool stop_;
};

#else

class ThreadPool {
public:
    explicit ThreadPool(size_t /*num_threads*/) {
    }

    void Enqueue(std::function<void()> task) {
        task();
    }
};

#endif
