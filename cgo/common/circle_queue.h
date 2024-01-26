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
#include <list>

// 只适用于有默认构造函数的结构
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
        _head = (T*)new T[_cap];
    }

    ~cqueue() {
        clear();
        delete []_head;
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
        *p = v;
        _write = (_write + 1) % _cap;
        return true;
    }

    bool pop(T& v) {
        if (empty()) {
            return false;
        }

        T* p = &_head[_read];
        _read = (_read + 1) % _cap;
        v = *p;
        return true;
    }

    const T& front() {
        return _head[_read];
    }
};

template<typename T>
class cqueue2 {
    std::list<T> _data;
    size_t _size;

public:
    cqueue2(size_t size) {
        _size = size;
    }

    bool full() {
        return _data.size() == _size;
    }

    bool empty() {
        return _data.empty();
    }

    bool push(const T& v) {
        if (full()) {
            return false;
        }
        _data.push_back(v);
        return true;
    }

    bool pop(T& v) {
        if (empty()) {
            return false;
        }
        v = _data.front();
        _data.pop_front();
        return true;
    }
};
