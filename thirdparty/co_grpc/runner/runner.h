//
// Created by xiaoqj on 2023/5/16.
//

#pragma once

#include <functional>
#include "cgo/cgo.h"

namespace co_grpc {

    struct NormalRunner {
        void operator()(std::function<void()> op) {
            op();
        }
    };

    struct CoRunner {
        void operator()(std::function<void()> op) {
            // 默认使用64k栈
            go op;
        }
    };

    struct CoWaiter {
        CoWaiter(uint64_t co_id) {
            co_id_ = co_id;
            assert(co_id_ != (uint64_t)-1);
        }

        CoWaiter() {
            co_id_ = cgocoid();
            assert(co_id_ != (uint64_t)-1);
        }

        void wait() {
            cgoyield();
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