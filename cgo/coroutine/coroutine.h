/*----------------------------------------------------------------
// Copyright 2021
// All rights reserved.
//
// author: 404558965@qq.com (xiaoquanjie)
// github: https://github.com/xiaoquanjie/cgo
// Created by xiaoqj on 2023/10/31.
//----------------------------------------------------------------*/

#pragma once

#include <functional>

namespace cgo::coroutine {
    // create one new coroutine
    uint64_t create(const std::function<void()> &routine, int stack = 0);

    bool set_udata(uint64_t co_id, void *data);

    void *get_udata(uint64_t co_id);

    // resume a coroutine, return the status of coroutine
    int resume(uint64_t co_id);

    void yield(uint64_t co_id);

    // yield
    void yield();

    uint64_t curid();

    void run(const std::function<void()> &routine, int stack);

    // create one new coroutine to run routine
    // when routine return, the coroutine will be destroyed
    void run(const std::function<void()> &routine);

    int stack_size(uint64_t co_id);
}