/*----------------------------------------------------------------
// Copyright 2021
// All rights reserved.
//
// author: 404558965@qq.com (xiaoquanjie)
// github: https://github.com/xiaoquanjie/cgo
// Created by xiaoqj on 2023/11/04
//----------------------------------------------------------------*/

#pragma once

#include <cstdlib>
#include <functional>

template<typename T>
class slist {
protected:
    struct node {
        T _val;
        node* _next = 0;
    };

    node* _head = 0;
    node* _tail = 0;
    size_t _size = 0;

public:
    slist() {
        _tail = _head = new node;
    }

    ~slist() {
        clear();
        delete _head;
    }

    [[nodiscard]]
    bool empty() const {
        return _size == 0;
    }

    [[nodiscard]]
    size_t size() const {
        return _size;
    }

    T& front() {
        return _head->_next->_val;
    }

    void pop() {
        if (_head->_next) {
            auto n = _head->_next;
            _head->_next = n->_next;
            if (_tail == n) {
                _tail = _head;
            }
            delete n;
            _size -= 1;
        }
    }

    bool pop(T& v) {
        if (empty()) {
            return false;
        }

        v = front();
        pop();
        return true;
    }

    void push(const T& val) {
        auto n = new node;
        n->_val = val;
        _tail->_next = n;
        _tail = n;
        _size += 1;
    }

    void clear() {
        auto n = _head->_next;
        while (n) {
            auto t = n;
            n = n->_next;
            delete t;
        }
        _head->_next = 0;
        _tail = _head;
        _size = 0;
    }

    // fn: return false and delete the element
    void iterate(const std::function<bool(T&)>& fn, bool once = false) {
        auto prev = _head;
        auto cur = prev->_next;
        while (cur) {
            if (!fn(cur->_val)) {
                prev->_next = cur->_next;
                delete cur;
                this->_size -= 1;
                if (_tail == cur) {
                    _tail = prev;
                    break;
                } else {
                    cur = prev->_next;
                }

                if (once) {
                    break;
                }
            } else {
                prev = cur;
                cur = cur->_next;
            }
        }
    }

    void swap(slist<T>& other) {
        auto tmp_ptr = this->_head;
        this->_head = other._head;
        other._head = tmp_ptr;

        tmp_ptr = this->_tail;
        this->_tail = other._tail;
        other._tail = tmp_ptr;

        auto tmp_size = this->_size;
        this->_size = other._size;
        other._size = tmp_size;
    }

    slist(const slist&) = delete;
    slist& operator=(const slist&) = delete;
};