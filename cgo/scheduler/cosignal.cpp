//
// Created by xiaoqj on 2024/1/26.
//

#include "cosignal.h"
#include "common/semaphore.h"
#include "scheduler.h"
#include <cassert>

namespace cgo {
    struct co_sig {
        Semaphore sem;
        void* volatile data = nullptr;
    };

    unsigned long long co_signal::id() const {
        return _id;
    }

    void co_signal::init() {
        auto id = scheduler::cur_coid();
        init(id);
    }

    void co_signal::init(unsigned long long id) {
        _id = id;
        if (_id == (uint64_t)-1) {
            if (!_sig) {
                _sig = (void*)new co_sig;
            } else {
                assert(false);
            }
        }
    }

    void co_signal::wait(void*&data) {
        if (_id == (uint64_t)-1) {
            ((co_sig*)_sig)->sem.wait();
            data = ((co_sig*)_sig)->data;
        } else {
            scheduler::schedule_wait_signal(data);
        }
    }

    void co_signal::wait() {
        void* data;
        wait(data);
    }

    void co_signal::post(void*data) {
        if (_id == (uint64_t)-1) {
            ((co_sig*)_sig)->data = data;
            ((co_sig*)_sig)->sem.post();
        } else {
            scheduler::schedule_post_signal(_id, data);
        }
    }

    void co_signal::post() {
        post(nullptr);
    }

    void co_signal::close() {
        if (_sig) {
            delete (co_sig*)_sig;
            _sig = nullptr;
        }
    }
}
