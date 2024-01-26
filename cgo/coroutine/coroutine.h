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
        extern thread_local volatile uint64_t gcurno;

        // create one new coroutine
        uint64_t create(const std::function<void()>& routine, int stack = 0);

        bool set_udata(uint64_t co_id, void* data);

        void* get_udata(uint64_t co_id);

        // resume a coroutine, return the status of coroutine
        int resume(uint64_t co_id);

        void yield(uint64_t co_id);

        // yield
        void yield();

        inline uint64_t curid() {
            uint64_t id = gcurno;
            return id;
        }

        void run(std::function<void()> routine, int stack);

        // create one new coroutine to run routine
        // when routine return, the coroutine will be destroyed
        void run(std::function<void()> routine);

        // memory in used
        int memory();
    }
}