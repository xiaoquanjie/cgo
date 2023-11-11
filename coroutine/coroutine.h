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
        void resume(int64_t co_id, void* data = 0);

        // yield
        void yield();

        void yield(void** data);

        int64_t curid();

        bool suspend_wait(int64_t co_id);

        // number of running coroutine
        int num();

        void run(std::function<void()> routine, int stack, const char* file, int line);

        // create one new coroutine to run routine
        // when routine return, the coroutine will be destroyed
        void run(std::function<void()> routine);

        // memory in used
        int memory();
    }
}