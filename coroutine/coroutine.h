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

namespace cgo {
    namespace coroutine {
        // create one new coroutine
        int64_t create(std::function<void()> routine, int stack = 0, const char* file = 0, int line = 0);

        // resume a coroutine
        void resume(int64_t co_id);

        // yield
        void yield();

        int64_t curid();

        // number of running coroutine
        int num();

#ifdef M_PLATFORM_WIN
        int num_in_thread();
#endif

        void run(std::function<void()> routine, int stack, const char* file, int line);

        // create one new coroutine to run routine
        // when routine return, the coroutine will be destroyed
        void run(std::function<void()> routine);

        // memory in used
        int memory();
    }
}