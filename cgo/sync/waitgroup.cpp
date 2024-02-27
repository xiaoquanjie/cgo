//
// Created by xiaoqj on 2024/2/27.
//

#include "waitgroup.h"
#include "scheduler/cosignal.h"
#include <assert.h>

namespace cgo {
    void WaitGroup::Add(int delta) {
        auto state = this->_state.fetch_add((uint64_t)delta << 32);
        state += (uint64_t)delta << 32;
        auto v = (int32_t)(state >> 32);
        auto w = (uint32_t)(state);

        if (v < 0) {
            throw "[cgo.WaitGroup]: negative WaitGroup counter";
        }
        if (w != 0 && delta > 0 && v == (int32_t)delta) {
            throw "[cgo.WaitGroup]: WaitGroup misuse: Add called concurrently with Wait";
        }
        if (v > 0 || w == 0) {
            return;
        }
        // This goroutine has set counter to 0 when waiters > 0.
        // Now there can't be concurrent mutations of state:
        // - Adds must not happen concurrently with Wait,
        // - Wait does not increment waiters if it sees counter == 0.
        // Still do a cheap sanity check to detect WaitGroup misuse.
        if (this->_state.load() != state) {
            throw "[cgo.WaitGroup]: WaitGroup misuse: Add called concurrently with Wait";
        }
        this->_state.store(0);
        assert(w == 1);
        auto sig = (co_signal*)this->_waiter;
        sig->post();
    }

    void WaitGroup::Done() {
        this->Add(-1);
    }

    void WaitGroup::Wait() {
        for (;;) {
            auto state = this->_state.load();
            auto v = (int32_t)(state >> 32);
            auto w = (uint32_t)(state);
            if (v == 0) {
                return;
            }
            if (w != 0) {
                throw "[cgo.WaitGroup]: WaitGroup misuse: call Wait over one time";
            }

            co_signal sig;
            sig.init();
            this->_waiter = &sig;
            if (this->_state.compare_exchange_weak(state, state+1)) {
                sig.wait();
                sig.close();

                if (this->_state.load() != 0) {
                    throw "[cgo.WaitGroup]: WaitGroup is reused before previous Wait has returned";
                }
                return;
            }
            sig.close();
        }
    }
}