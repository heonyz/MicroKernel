#pragma once

#include <atomic>
#include <future>
#include <memory>
#include <list>
#include <mutex>
#include <thread>
#include <condition_variable>
#include "micro_sync_task_queue.hpp"
#include "thread_pool.hpp"


class MicroKernelThreadPool : public IThreadPool {
public:
    MicroKernelThreadPool(size_t task_limit = 100,
        int thread_cnt = std::thread::hardware_concurrency())
        : queue_(task_limit), running_(false) {
        running_ = true;
        for (int i = 0; i < thread_cnt; i++) {
            threads_.push_back(
                std::make_shared<std::thread>([this] { run(); }));
        }
    }

    virtual ~MicroKernelThreadPool() { stop(); }

    virtual void run() override {
        while (running_) {
            thread_task_t t = nullptr;
            bool ret = queue_.pop(t);
            if (!ret || !t || !running_) {
                return;
            }
            t();
        }
    }

    virtual void stop() override {
        std::call_once(flag_, [this] { _stop(); });
    }

    virtual void add_task(const thread_task_t& task) override {
        queue_.push([task]() { task(); });
    }

private:
    void _stop(void) {
        queue_.stop();
        running_ = false;
        for (auto thread : threads_) {
            if (thread) {
                thread->join();
            }
        }
        threads_.clear();
    }

private:
    std::list<std::shared_ptr<std::thread>> threads_;
    MicroSyncTaskQueue<thread_task_t> queue_;
    std::atomic_bool running_;
    std::once_flag flag_;
};

