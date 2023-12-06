/*----------------------------------------------------------------
// Copyright 2021
// All rights reserved.
//
// author: 404558965@qq.com (xiaoquanjie)
// github: https://github.com/xiaoquanjie/cgo
// Created by xiaoqj on 2023/11/10
//----------------------------------------------------------------*/

#pragma once

#include <stdlib.h>
#include <atomic>

template<typename T>
class cqueue {
protected:
    T* _head = 0;
    size_t _cap = 1;
    size_t _read = 0;
    size_t _write = 0;
public:
    cqueue(size_t cap) {
        if (cap > 0) {
            _cap = cap + 1;
        }
        _head = (T*)malloc(sizeof(T)*_cap);
    }

    ~cqueue() {
        clear();
        free(_head);
    }

    void clear() {
        for (; _read != _write; _read++) {
            _read = (_read + 1) % _cap;
            T* p = &(this->_head[_read]);
            p->~T();
        }
        _read = _write = 0;
    }

    size_t cap() const {
        return _cap - 1;
    }

    size_t size() const {
        return (_write + _cap - _read) % _cap;
    }

    bool full() const {
        return (_write + 1) % _cap == _read;
    }

    bool empty() const {
        return _read == _write;
    }

    bool push(const T& v) {
        if (full()) {
            return false;
        }

        T* p = &_head[_write];
        _write = (_write + 1) % _cap;
        new(p)T(v);
        return true;
    }

    bool pop(T& v) {
        if (empty()) {
            return false;
        }

        T* p = &_head[_read];
        _read = (_read + 1) % _cap;
        v = *p;
        p->~T();
        return true;
    }

    const T& front() {
        return _head[_read];
    }
};

// only for inbuilt type
template<typename T>
class cas_cqueue {
protected:
    T* _head = 0;
    size_t _cap = 1;
    std::atomic_uint32_t _read = 0;
    std::atomic_uint32_t _write = 0;

public:
    cas_cqueue(size_t cap) {
        if (cap > 0) {
            _cap = cap + 1;
        }
        _head = (T*)malloc(sizeof(T)*_cap);
    }

    ~cas_cqueue() {
        ::free(_head);
    }

    size_t cap() const {
        return _cap - 1;
    }

    size_t size() const {
        auto w = _write.load(std::memory_order_relaxed);
        auto r = _read.load(std::memory_order_relaxed);
        return (w + _cap - r) % _cap;
    }

    bool full() const {
        auto w = _write.load(std::memory_order_relaxed);
        auto r = _read.load(std::memory_order_relaxed);
        return (w + 1) % _cap == r;
    }

    bool empty() const {
        return _read == _write;
    }

    bool push(const T& v) {
        for (;;) {
            uint32_t cmp = _read.load(std::memory_order_relaxed);
            uint32_t oldv = _write.load(std::memory_order_relaxed);
            if ((oldv + 1) % _cap == cmp) {
                return false;
            }

            uint32_t newv = (oldv+1) % _cap;
            if (_write.compare_exchange_strong(oldv, newv, std::memory_order_seq_cst)) {
                _head[oldv] = v;
                break;
            }
        }

        return true;
    }

    bool pop(T& v) {
        for (;;) {
            // unequal and swap
            uint32_t cmp = _write.load(std::memory_order_relaxed);
            uint32_t oldv = _read.load(std::memory_order_relaxed);
            if (oldv == cmp) {
                return false;
            }

            uint32_t newv = (oldv+1) % _cap;
            if (_read.compare_exchange_strong(oldv, newv, std::memory_order_seq_cst)) {
                v = _head[oldv];
                break;
            }
        }
        return true;
    }

};