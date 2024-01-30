//
// Created by xiaoqj on 2024/1/15.
//

#include "mutex.h"
#include "scheduler/cosignal.h"
#include "scheduler/scheduler.h"
#include "common/mpmcqueue_ex.h"
#include "common/print.h"

#define M_WAITERS(waiter) ((co_waiter_list*)waiter)

namespace cgo {
    using co_waiter_list = MPMCQueueEx<cgo::signal>;

    co_mutex::co_mutex() {
        _waiters = (void*)new co_waiter_list(8);
        _lock.clear();
        _owner = 0;
    }

    co_mutex::~co_mutex() {
        delete ((co_waiter_list*)_waiters);
    }

    void co_mutex::lock() {
        if (try_lock()) {
            return;
        }

        cgo::signal self;
        self.init();
        if (self.id() == _owner) {
            throw "not recursive lock";
        }

        M_WAITERS(_waiters)->push(self);
        if (!try_resume(&self)) {
            // 挂起
            self.wait();
        }
        self.close();
    }

    // 不允许重入
    bool co_mutex::try_lock() {
        for (int i = 0; i < 1; i++) {
            if (!_lock.test_and_set()) {
                assert(_owner == 0);
                _owner = scheduler::cur_coid();
                return true;
            }
        }
        return false;
    }

    void co_mutex::unlock() {
        auto id = scheduler::cur_coid();
        if (id != _owner) {
            throw "not lock owner";
        }

        cgo::signal ot;
        if (M_WAITERS(_waiters)->try_pop(ot)) {
            // 更换所有者
            _owner = ot.id();
            ot.post();
        } else {
            _owner = 0;
            _lock.clear();
        }
    }

    bool co_mutex::try_resume(void* task) {
        if (_lock.test_and_set()) {
            return false;
        }

        cgo::signal ot;
        if (!M_WAITERS(_waiters)->try_pop(ot)) {
            throw "lock error";
        }

        _owner = ot.id();
        if (ot.id() == ((cgo::signal*)task)->id()) {
            return true;
        }

        ot.post();
        return false;
    }
}