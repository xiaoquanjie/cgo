//
// Created by xiaoqj on 2023/5/16.
//

#pragma once

#include <functional>
#include "cgo/cgo.h"

#undef GoRun
#define GoRun go

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
        CoWaiter() {
            co_id_ = cgocoid();
            assert(co_id_ != (uint64_t)-1);
        }

        void wait() {
            cgoyield();
        }

        void resume() const {
            cgoresume(co_id_);
        }

        uint64_t co_id_ = 0;
    };

    struct CoLooper {
        void operator()(std::function<void()> op) {
            cgoloop(op);
        }
    };

    struct CoMutex {
        cgo::mutex mu_;

        inline void lock() {
            this->mu_.lock();
        }

        inline void unlock() {
            this->mu_.unlock();
        }
    };

    struct CoScopedLock {
        CoMutex& mu_;

        CoScopedLock(CoMutex& mu) : mu_(mu) {
            mu.lock();
        }

        ~CoScopedLock() {
            mu_.unlock();
        }
    };

    template<typename T>
    struct PointScoped {
        T* data_;
        PointScoped(T* p) : data_(p) {}
        ~PointScoped() { delete data_; }
        T* operator->() { return data_;}
    };
}