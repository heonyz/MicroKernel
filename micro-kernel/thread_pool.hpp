#pragma once
#include <functional>



typedef std::function<void(void)> thread_task_t;

class IThreadPool {
public:
    virtual ~IThreadPool() {}
    virtual void run() = 0;
    virtual void stop() = 0;
    virtual void add_task(const thread_task_t& task) = 0;
};


