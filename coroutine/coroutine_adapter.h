/*----------------------------------------------------------------
// Copyright 2021
// All rights reserved.
//
// author: 404558965@qq.com (xiaoquanjie)
// github: https://github.com/xiaoquanjie/cgo
// Created by xiaoqj on 2023/12/15
//----------------------------------------------------------------*/

#pragma once

#include <stdint.h>
#include <functional>

#define USE_CGO_COROUTINE // use cgo builtin coroutine
//#define USE_MINI_CORO     // use minicoro

namespace cgo {
    namespace coro_adapter {
        uint64_t create_co(std::function<void()> routine, int stack = 0, const char* file = 0, int line = 0);

        void resume_co(uint64_t co_id, void* data);

        void yield_co(void*& data);

        void yield_co();

        void run_co(std::function<void()> routine, int stack = 0, const char* file = 0, int line = 0);

        uint64_t cur_coid();
    }
}