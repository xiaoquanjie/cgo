//
// Created by xiaoqj on 2024/1/15.
//

#include "mutex.h"
#include "scheduler/cosignal.h"
#include "common/mpmcqueue_ex.h"
#include "common/print.h"

#define M_WAITERS(waiter) ((co_waiter_list*)waiter)
#define M_GET_SELF(id) id = scheduler::cur_coid(); assert(id != 0);
#define M_IN_CO(tid) (tid != (unsigned long long)-1)

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
            throw
        }

        auto id = 0;
        M_GET_SELF(id);
        if (id == _owner) {
            throw "not recursive lock";
        }

//        fill_task(&task);
//
//        if (M_IN_CO(task.tid)) {
//            void* data;
//            scheduler::schedule_yield(data, [this, task] {
//                M_WAITERS(_waiters)->push(task);
//                if (try_resume(&task)) {
//                    scheduler::schedule_co(task.tid, 0);
//                }
//            });
//        } else {
//            M_WAITERS(_waiters)->push(task);
//            if (!try_resume(&task)) {
//                ((Semaphore*)task.sem)->wait();
//            }
//            delete (Semaphore*)task.sem;
//        }
    }

    // 不允许重入
    bool co_mutex::try_lock() {
        for (int i = 0; i < 1; i++) {
            if (!_lock.test_and_set()) {
                assert(_owner == 0);
                M_GET_SELF(_owner);
                return true;
            }
        }
        return false;
    }

    void co_mutex::unlock() {
//        task_struct task;
//        M_GET_SELF(task.tid);
//        if (task.tid != _owner) {
//            throw "not lock owner";
//        }
//
//        if (M_WAITERS(_waiters)->try_pop(task)) {
//            // 更换所有者
//            _owner = task.tid;
//            resume_task(&task);
//        } else {
//            _owner = 0;
//            _lock.clear();
//        }
    }

    bool co_mutex::try_resume(const void* task) {
//        if (_lock.test_and_set()) {
//            return false;
//        }
//
//        task_struct old_task;
//        if (!M_WAITERS(_waiters)->try_pop(old_task)) {
//            throw "lock error";
//        }
//
//        // 更换所有者
//        _owner = old_task.tid;
//        if (old_task.tid == ((const task_struct*)task)->tid) {
//            return true;
//        }
//
//        resume_task(&old_task);
//        return false;
    }
}