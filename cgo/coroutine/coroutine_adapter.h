/*----------------------------------------------------------------
// Copyright 2021
// All rights reserved.
//
// author: 404558965@qq.com (xiaoquanjie)
// github: https://github.com/xiaoquanjie/cgo
// Created by xiaoqj on 2023/12/15
//----------------------------------------------------------------*/

#pragma once

#include <cstdint>
#include <functional>

#define USE_CGO_COROUTINE // use cgo builtin coroutine
//#define USE_MINI_CORO     // use minicoro

namespace cgo::coro_adapter {
    uint64_t create_co(const std::function<void()>& routine, int stack = 0, const char* file = nullptr, int line = 0);

    // 唤醒协程
    void resume_co(uint64_t co_id);

    // 协程等待信号量
    void co_wait_signal(void*& data);

    // 协程投递信号量
    void co_post_signal(uint64_t co_id, void* data);

    void run_co(const std::function<void()>& routine, int stack = 0, const char* file = nullptr, int line = 0);

    uint64_t cur_coid();

    void co_hook(bool enable);

    bool co_hook();

    int co_stack(uint64_t co_id);
}