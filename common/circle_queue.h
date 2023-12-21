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
    struct node {
        T _val;
        bool _flag = false;
    };

    size_t _full = 0;
    size_t _cap = 0;
    node* _head = 0;
    std::atomic_uint16_t _read = 0;
    std::atomic_uint16_t _write = 0;

public:
    cas_cqueue(size_t cap) {
        _full = cap;
        if (cap & (cap - 1)) {
            cap = roundup_pow_of_two(cap);
        }

        _cap = cap + 1;
        _head = new node[_cap];
    }

    ~cas_cqueue() {
        delete []_head;
    }

    size_t cap() const {
        return _cap - 1;
    }

//    size_t size() const {
//        return (_write + _cap - _read) % _cap;
//    }
//
//    bool full() const {
//        return (_write + 1) % _cap == _read;
//    }
//
//    bool empty() const {
//        return _read == _write;
//    }

    bool enqueue(const T& v) {
        return push(v);
    }

    bool dequeue(T& v) {
        return pop(v);
    }

    bool push(const T& v) {
        for (;;) {
            auto cmp = _read.load();
            auto oldv = _write.load();

            // oldv + _full
            if ((oldv+1) % _cap == (cmp%_cap)) {
                return false;
            }

            if (_write.compare_exchange_weak(oldv, oldv+1)) {
                oldv = oldv % _cap;
                while (_head[oldv]._flag);
                _head[oldv]._val = v;
                std::atomic_thread_fence(std::memory_order_release);
                _head[oldv]._flag = true;
                return true;
            }
        }
        return true;
    }

    bool pop(T& v) {
        for (;;) {
            auto oldv = _read.load();
            auto cmp = _write.load();

            if (oldv == cmp) {
                return false;
            }

            if (_read.compare_exchange_weak(oldv, oldv + 1)) {
                oldv = oldv % _cap;
                while (!_head[oldv]._flag); 
                v = _head[oldv]._val;
                std::atomic_thread_fence(std::memory_order_release);
                _head[oldv]._flag = false;
                return true;
            }
        }
        return true;
    }

private:
    size_t roundup_pow_of_two(size_t number) {
        if (number == 0)
            return 1;

        number--;
        number |= number >> 1;
        number |= number >> 2;
        number |= number >> 4;
        number |= number >> 8;
        number |= number >> 16;
        number |= number >> 32;
        number++;

        return number;
    }
};

template<typename T>
class mpmc_bounded_queue
{
public:
    mpmc_bounded_queue(size_t buffer_size)
    {
        if (buffer_size & (buffer_size - 1)) {
            buffer_size = roundup_pow_of_two(buffer_size);
        }

        assert((buffer_size >= 2) &&
               ((buffer_size & (buffer_size - 1)) == 0));

        buffer_ = new cell_t [buffer_size];
        buffer_mask_ = buffer_size - 1;

        for (size_t i = 0; i != buffer_size; i += 1)
            buffer_[i].sequence_.store(i, std::memory_order_relaxed);

        enqueue_pos_.store(0, std::memory_order_relaxed);
        dequeue_pos_.store(0, std::memory_order_relaxed);
    }

    ~mpmc_bounded_queue()
    {
        delete [] buffer_;
    }

    bool enqueue(T const& data)
    {
        cell_t* cell;
        size_t pos = enqueue_pos_.load(std::memory_order_relaxed);
        for (;;)
        {
            cell = &buffer_[pos & buffer_mask_];
            size_t seq =
                    cell->sequence_.load(std::memory_order_acquire);

            intptr_t dif = (intptr_t)seq - (intptr_t)pos;
            if (dif == 0)
            {
                if (enqueue_pos_.compare_exchange_weak
                        (pos, pos + 1, std::memory_order_relaxed))
                    break;
            }
            else if (dif < 0)
                return false;
            else
                pos = enqueue_pos_.load(std::memory_order_relaxed);

        }

        cell->data_ = data;
        cell->sequence_.store(pos + 1, std::memory_order_release);
        return true;
    }

    bool dequeue(T& data)
    {
        cell_t* cell;
        size_t pos = dequeue_pos_.load(std::memory_order_relaxed);

        for (;;)
        {
            cell = &buffer_[pos & buffer_mask_];
            size_t seq =
                    cell->sequence_.load(std::memory_order_acquire);

            intptr_t dif = (intptr_t)seq - (intptr_t)(pos + 1);
            if (dif == 0)
            {
                if (dequeue_pos_.compare_exchange_weak
                        (pos, pos + 1, std::memory_order_relaxed))
                    break;
            }
            else if (dif < 0)
                return false;
            else
                pos = dequeue_pos_.load(std::memory_order_relaxed);
        }

        data = cell->data_;
        cell->sequence_.store
                (pos + buffer_mask_ + 1, std::memory_order_release);

        return true;
    }

protected:
    size_t roundup_pow_of_two(size_t number) {
        if (number == 0)
            return 1;

        number--;
        number |= number >> 1;
        number |= number >> 2;
        number |= number >> 4;
        number |= number >> 8;
        number |= number >> 16;
        number |= (size_t)(number >> 32);
        number++;

        return number;
    }

private:
    struct cell_t
    {
        std::atomic<size_t>   sequence_;
        T                     data_;
    };

    static size_t const     cacheline_size = 64;
    typedef char            cacheline_pad_t [cacheline_size];
    cacheline_pad_t         pad0_;
    cell_t*                 buffer_;
    size_t                  buffer_mask_;
    cacheline_pad_t         pad1_;
    std::atomic<size_t>     enqueue_pos_;
    cacheline_pad_t         pad2_;
    std::atomic<size_t>     dequeue_pos_;
    cacheline_pad_t         pad3_;

    mpmc_bounded_queue(mpmc_bounded_queue const&);
    void operator = (mpmc_bounded_queue const&);

};

template<typename T>
class cqueue2 {
protected:
    T* _head = 0;
    size_t _cap = 1;
    size_t _read = 0;
    size_t _write = 0;
    std::mutex _mu;

public:
    cqueue2(size_t cap) {
        if (cap > 0) {
            _cap = cap + 1;
        }
        _head = (T*)malloc(sizeof(T)*_cap);
    }

    ~cqueue2() {
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

    bool enqueue(const T& v) {
        std::unique_lock<std::mutex> lock(_mu);
        if (full()) {
            return false;
        }

        T* p = &_head[_write];
        _write = (_write + 1) % _cap;
        new(p)T(v);
        return true;
    }

    bool dequeue(T& v) {
        std::unique_lock<std::mutex> lock(_mu);
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