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
        uint64_t create(std::function<void()> routine, int stack = 0, const char* file = 0, int line = 0);

        bool set_udata(uint64_t co_id, void* data);

        void* get_udata(uint64_t co_id);

        // resume a coroutine, return the status of coroutine
        int resume(uint64_t co_id);

        // yield
        void yield();

        uint64_t curid();

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