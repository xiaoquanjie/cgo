//
// Created by xiaoqj on 2024/1/25.
//

#pragma once

#include "mpmcqueue.h"
#include <shared_mutex>

template<typename T>
using MPMCQueue = rigtorp::MPMCQueue<T>;

template<typename T>
class MPMCQueueEx {
    using queue = MPMCQueue<T>;
    size_t _size;
    queue* volatile _que;
    std::shared_mutex _mu;

public:
    explicit MPMCQueueEx(size_t init_capactiy) : _size(init_capactiy) {
        if (_size < 2) _size = 2;
        _que = new_queue(nullptr, _size);
    }

    ~MPMCQueueEx() {
        delete _que;
    }

    void push(const T &v) {
        do {
            _mu.lock_shared();
            for (int i = 0; i < 1; i++) {
                if (_que->try_push(v)) {
                    _mu.unlock_shared();
                    return;
                }
            }
            _mu.unlock_shared();

            size_t oldsize = _size;

            _mu.lock();
            if (oldsize == _size) {
                _size = (size_t)((double)oldsize * 1.5);
                _que = new_queue(_que, _size);
            }
            _mu.unlock();
        } while (true);
    }

    inline bool try_pop(T &v) {
        _mu.lock_shared();
        auto ret = _que->try_pop(v);
        _mu.unlock_shared();
        return ret;
    }

private:
    queue* new_queue(queue* from, size_t s) {
        auto newq = new queue(s);
        if (from) {
            T v;
            while (from->try_pop(v)) {
                newq->try_push(v);
            }
            assert(from->empty());
            delete from;
        }
        return newq;
    }
};
