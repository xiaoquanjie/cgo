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
#include <mutex>

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
    using lock_type = std::atomic_bool;
    T* _head = 0;
    size_t _cap = 1;
    uint32_t volatile _read = 0;
    uint32_t volatile _write = 0;
    lock_type _rflag;
    lock_type _wflag;

public:
    cas_cqueue(size_t cap) {
        if (cap > 0) {
            _cap = cap + 1;
        }
        _head = (T*)malloc(sizeof(T)*_cap);
        _wflag.store(false);
        _rflag.store(false);
    }

    ~cas_cqueue() {
        ::free(_head);
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
        for (;;) {
            if (full()) {
                return false;
            }
            if (!lock(_wflag)) {
                continue;
            }
            if (full()) {
                unlock(_wflag);
                return false;
            }
            break;
        }

        _head[_write] = v;
        std::atomic_thread_fence(std::memory_order_release);
        _write = (_write + 1) % _cap;

        unlock(_wflag);
        return true;
    }

    bool pop(T& v) {
        for (;;) {
            if (empty()) {
                return false;
            }
            if (!lock(_rflag)) {
                continue;
            }
            if (empty()) {
                unlock(_rflag);
                return false;
            }
            break;
        }

        v = _head[_read];
        _read = (_read + 1) % _cap;

        unlock(_rflag);
        return true;
    }

private:
    inline bool lock(lock_type& flag) {
        bool expected = false;
        if (flag.compare_exchange_weak(expected, true, std::memory_order_relaxed)) {
            return true;
        }
        return false;
    }

    inline bool unlock(lock_type& flag) {
        bool expected = true;
        assert(flag.compare_exchange_strong(expected, false, std::memory_order_relaxed));
        return true;
    }
};

// only for inbuilt type
template<typename T>
class cas_cqueue2 {
protected:
    size_t _cap = 0;
    T* _head = 0;
    std::atomic_uint32_t _read = 0;
    std::atomic_uint32_t _write = 0;

public:
    cas_cqueue2(size_t cap) {
        if (cap & (cap - 1)) {
            assert(false);
        }

        _cap = cap + 1;
        _head = (T*)malloc(sizeof(T)*_cap);
    }

    ~cas_cqueue2() {
        ::free(_head);
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
        for (;;) {
            uint32_t cmp = _read.load();
            uint32_t oldv = _write.load();
            if ((oldv+1) % _cap == cmp) {
                return false;
            }

            if (_write.compare_exchange_weak(oldv, oldv+1)) {
                assert(_cap != 0);
                oldv = oldv % _cap;
                assert(oldv < _cap);
                _head[oldv] = v;
                std::atomic_thread_fence(std::memory_order_release);
                break;
            }
        }
        return true;
    }

    bool pop(T& v) {
        for (;;) {
            uint32_t cmp = _write.load();
            uint32_t oldv = _read.load();
            if (cmp == oldv) {
                return false;
            }

            if (_read.compare_exchange_weak(oldv, oldv + 1)) {
                assert(_cap != 0);
                oldv = oldv % _cap;
                assert(oldv < _cap);
                v = _head[oldv];
                break;
            }
        }
        return true;
    }
};