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

    bool full() {
        return (_write + 1) % _cap == _read;
    }

    bool empty() {
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