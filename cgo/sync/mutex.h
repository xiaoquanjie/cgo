//
// Created by xiaoqj on 2024/1/15.
//

#pragma once

#include <atomic>

namespace cgo {
    // 不支持可重入
    // 不可以设置优先级，基本上是按顺序抢占
    class co_mutex {
    protected:
        std::atomic_flag _lock;
        void* _waiters;
        volatile unsigned long long _owner;

        bool try_resume(const void* task);
    public:
        co_mutex();

        ~co_mutex();

        co_mutex(const co_mutex&) = delete;
        co_mutex& operator=(const co_mutex&) = delete;

        void lock();

        bool try_lock();

        void unlock();
    };

    // alias
    using mutex = co_mutex;
}
