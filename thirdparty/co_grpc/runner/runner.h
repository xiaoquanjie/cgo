//
// Created by xiaoqj on 2023/5/16.
//

#pragma once

#include <functional>
#include "cgo/cgo.h"

namespace co_grpc {

struct CoRunner {
    void operator()(std::function<void()> op) {
        go op;
    }
};

struct CoWaiter {
    CoWaiter() {
        co_id_ = cgocoid();
        assert(co_id_ != -1);
    }

    // 此操作必须在协程里
    template<class Func>
    void wait(Func after) {
        void* data = 0;
        cgoyield(data, after);
    }

    void Resume() const {
        cgoresume(co_id_);
    }

    uint64_t co_id_ = 0;
};

struct CoLooper {
    void operator()(std::function<void()> op) {
        cgoloop(op);
    }
};

}