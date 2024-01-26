//
// Created by xiaoqj on 2024/1/25.
//

#pragma once

#include <atomic>
#include <assert.h>

class SpinRwLock {
protected:
    std::atomic_uint16_t state;

public:
    SpinRwLock() {
        state = 0;
    }

    inline void rlock() {
        uint16_t c;
        for (;;) {
            c = state.load(std::memory_order_relaxed);
            if (c & (1 << 15))
                continue;
            if (state.compare_exchange_weak(c, c + 1, std::memory_order_relaxed)) {
                break;
            }
        }

//        c = state;
//        assert((c & (1 << 15)) == 0);
//        assert(c != 0);
    }

    inline void runlock() {
//        uint16_t c = state;
//        assert((c & (1 << 15)) == 0);

        state.fetch_sub(1, std::memory_order_relaxed);
    }

    inline void wlock() {
        uint16_t c;
        for (;;) {
            c = state.load(std::memory_order_relaxed);
            if (c)
                continue;
            if (state.compare_exchange_weak(c, 1 << 15, std::memory_order_relaxed))
                break;
        }

//        c = state;
//        assert(c == (1 << 15));
    }

    inline void wunlock() {
//        uint16_t c = state;
//        assert(c == (1 << 15));

        state.store(0, std::memory_order_relaxed);
    }
};
