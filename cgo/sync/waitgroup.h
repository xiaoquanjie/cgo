//
// Created by xiaoqj on 2024/2/27.
//

#pragma once

#include <atomic>

namespace cgo {
    class WaitGroup {
    protected:
        std::atomic_uint64_t _state = 0; // high 32 bits are counter, low 32 bits are waiter count.
        void* volatile _waiter = 0;

        WaitGroup(const WaitGroup&) = delete;
        WaitGroup& operator=(const WaitGroup&) = delete;

    public:
        WaitGroup() = default;

        void Add(int delta);

        void Done();

        // 暂时只支持同时只能有一个协程在wait
        void Wait();
    };
}
