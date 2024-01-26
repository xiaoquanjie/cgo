//
// Created by xiaoqj on 2024/1/25.
//

#pragma once

#include "mpmcqueue.h"
#include "rwlock.h"

template<typename T>
using MPMCQueue = rigtorp::MPMCQueue<T>;

template<typename T>
class MPMCQueueEx {
    using queue = MPMCQueue<T>;
    size_t _size;
    queue* volatile _que;
    SpinRwLock _rwlock;

public:
    MPMCQueueEx(size_t init_capactiy) : _size(init_capactiy) {
        if (_size < 2) _size = 2;
        _que = new_queue(nullptr, _size);
    }

    ~MPMCQueueEx() {
        delete _que;
    }

    void push(const T &v) {
        do {
            _rwlock.rlock();
            for (int i = 0; i < 1; i++) {
                if (_que->try_push(v)) {
                    _rwlock.runlock();
                    return;
                }
            }
            _rwlock.runlock();

            size_t oldsize = _size;

            _rwlock.wlock();
            if (oldsize == _size) {
                _size = (size_t)(oldsize * 1.5);
                _que = new_queue(_que, _size);
            }
            _rwlock.wunlock();
        } while (true);
    }

    inline bool try_pop(T &v) {
        _rwlock.rlock();
        auto ret = _que->try_pop(v);
        _rwlock.runlock();
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
            assert(from->size() == 0);
            delete from;
        }
        return newq;
    }
};
