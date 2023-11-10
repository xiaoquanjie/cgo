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
    size_t _cap = 0;
    size_t _read = 0;
    size_t _write = 0;

public:
    cqueue(size_t cap) {
        if (cap) {
            _cap = cap;
            _head = (T*)malloc(sizeof(T)*_cap);
        }
    }

    ~cqueue() {
        clear();
        free(_head);
    }

    void clear() {

    }
};