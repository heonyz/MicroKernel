#pragma once
#include <stddef.h>


template <typename T>
class ISyncQueue {
public:
    virtual ~ISyncQueue() {}
    virtual bool push(T&& obj) = 0;
    virtual bool pop(T& t) = 0;
    virtual size_t count(void) = 0;
    virtual bool empty(void) = 0;
    virtual bool full(void) = 0;
    virtual void stop(void) = 0;
};


