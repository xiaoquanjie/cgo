/*----------------------------------------------------------------
// Copyright 2021
// All rights reserved.
//
// author: 404558965@qq.com (xiaoquanjie)
// github: https://github.com/xiaoquanjie/cgo
// Created by xiaoqj on 2023/10/31.
//----------------------------------------------------------------*/

#pragma once

#include "macro.h"
#include "../common/concurrentqueue.h"
#include <functional>
#include <atomic>
#include <shared_mutex>

#ifdef M_PLATFORM_WIN
#include <Windows.h>
#else
#include <ucontext.h>
#endif

namespace cgo {
    using co_no_queue_type = moodycamel::ConcurrentQueue<int>;

    namespace coroutine {
        struct _schedule_st_;

        struct _co_st_ {
            volatile int _status = 0;
            std::function<void()> _routine;
            void* volatile _data = 0;
            int _ssize = 0;
            const char* _file = 0;
            int _line = 0;

#ifdef M_PLATFORM_WIN
            LPVOID _ctx = 0;
            LPVOID _mctx = 0;
#else
            ucontext_t _ctx;
            ucontext_t *_mctx = 0;
            char *_stack = 0;
#endif
            uint64_t get_coid();
            static _co_st_* alloc(std::function<void()> routine, int stack, const char* file, int line);
            static _co_st_* init(_co_st_*, std::function<void()> routine, int stack, const char* file, int line);
            static _co_st_* get_co(uint64_t co_id);
            static uint64_t get_coid(_co_st_*);
            static void free(_co_st_* co);
            static void free(uint64_t co_id);
            static bool memory_check(_co_st_* co);
        };

        extern thread_local volatile uint64_t gcurno;
#ifdef M_PLATFORM_WIN
        extern thread_local LPVOID volatile gmainctx;
#else
        extern thread_local ucontext_t* volatile gmainctx;
#endif

        struct _memory_st_ {
            std::atomic_int _mem = 0;
            void add(size_t s);
            void dec(size_t s);
        };

        extern _memory_st_ gmem;
    }
}
