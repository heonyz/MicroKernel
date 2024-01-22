#pragma once

#include <list>
#include <thread>
#include <mutex>
#include <condition_variable>
#include "sync_queue.hpp"
#include <utility>


template <typename T>
class MicroSyncTaskQueue : public ISyncQueue<T> {
public:
    MicroSyncTaskQueue(size_t size) : max_size_(size), stop_(false) {}
    virtual ~MicroSyncTaskQueue() { stop(); }

    virtual bool push(T&& obj) override {
        std::unique_lock<std::mutex> lck(mutex_);
        not_full_.wait(lck, [this] { return stop_ || (count() < max_size_); });

        if (stop_) {
            return false;
        }

        queue_.push_back(std::forward<T>(obj));
        not_empty_.notify_one();
        return true;
    }

    virtual bool pop(T& t) override {
        std::unique_lock<std::mutex> lck(mutex_);
        not_empty_.wait(lck, [this] { return stop_ || !queue_.empty(); });

        if (stop_) {
            return false;
        }

        t = queue_.front();
        queue_.pop_front();
        not_full_.notify_one();
        return true;
    }

    virtual size_t count(void) override { return queue_.size(); }
    virtual bool empty(void) override { return queue_.empty(); }
    virtual bool full(void) override { return queue_.size() >= max_size_; }

    virtual void stop(void) override {
        {
            std::unique_lock<std::mutex> lck(mutex_);
            stop_ = true;
        }
        not_full_.notify_all();
        not_empty_.notify_all();
    }

private:
    std::list<T> queue_;
    std::mutex mutex_;
    std::condition_variable not_empty_;
    std::condition_variable not_full_;
    size_t max_size_;
    bool stop_;
};

