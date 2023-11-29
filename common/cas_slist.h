/*----------------------------------------------------------------
// Copyright 2021
// All rights reserved.
//
// author: 404558965@qq.com (xiaoquanjie)
// github: https://github.com/xiaoquanjie/cgo
// Created by xiaoqj on 2023/11/28
//----------------------------------------------------------------*/

#pragma once

#include <atomic>

template<typename T>
class cas_slist {
protected:
    struct node {
        T _val;
        std::atomic<node*> _next = 0;
    };

    std::atomic<node*> _head = 0;
    std::atomic<node*> _tail = 0;
    std::atomic_uint32_t _size = 0;

protected:
    cas_slist(const cas_slist&) = delete;
    cas_slist& operator=(const cas_slist&) = delete;

public:
    cas_slist() {
        auto n = new node;
        _head.store(n, std::memory_order_relaxed);
        _tail.store(n, std::memory_order_relaxed);
    }

    ~cas_slist() {
        while (_head.load() != nullptr) {
            node* tmp = _head.load();
            _head.store(tmp->_next);
            delete tmp;
        }
    }

    bool empty() const {
        auto c = _size.load(std::memory_order_relaxed);
        return c == 0;
    }

    std::size_t size() const {
        auto c = _size.load(std::memory_order_relaxed);
        return c;
    }

    // todo: 存在aba问题需要解决
    void push(const T& val) {
        auto n = new node;
        n->_val = val;

        while (true) {
            // 先抢尾指针
            node* last = _tail.load(std::memory_order_relaxed);
            if (!last) {
                continue;
            }
            if (!_tail.compare_exchange_weak(last, nullptr)) {
                continue;
            }

            // 尾指针的尾部设置指向
            last->_next.store(n, std::memory_order_relaxed);
            _size.fetch_add(1);

            // 设置尾指针
            node* expected = nullptr;
            _tail.compare_exchange_weak(expected, n);
            break;
        }
    }

    bool pop(T& val) {
        while (true) {
            // 先抢头指针
            auto first = _head.load(std::memory_order_relaxed);
            auto next = first->_next;

            if (!_head.compare_exchange_weak(first, next)) {
                continue;
            }



            if (_head.compare_exchange_weak(first, nullptr)) {
                continue;
            }


        }

        return false;
    }
};
