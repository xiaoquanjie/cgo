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
#include "../common/squeue.hpp"
#include <functional>
#include <atomic>
#include <shared_mutex>

#ifdef M_PLATFORM_WIN
#include <Windows.h>
#else
#include <ucontext.h>
#endif

namespace cgo {
    namespace coroutine {
        struct _schedule_st_;

        struct _co_st_ {
            int _no = -1;
            int _status = 0;
            std::function<void()> _routine;
            _schedule_st_* _schedule = 0;
            const char* _file = 0;
            int _line = 0;
            int _ssize = 0;

#ifdef M_PLATFORM_WIN
            LPVOID _ctx = 0;
            LPVOID _mctx = 0;
#else
            ucontext_t _ctx;
            ucontext_t *_mctx = 0;
            char *_stack = 0;
            int _scap = 0;
            //char *_pstack = 0;
#endif

            static _co_st_* alloc(int stack);
            static void free(_co_st_* co);
        };

        struct _schedule_st_ {
            // capicity
            int _cap = 0;
            int _no = 0;
            _co_st_ **_co = 0;
            squeue<int> _freenos;
            std::shared_mutex _mu;

            ~_schedule_st_();
            _co_st_* alloc_co(std::function<void()> routine, int stack, const char* file, int line);
            void free_co(_co_st_* co);
            _co_st_* get_co(int64_t no);
        private:
            void realloc_schedule();
        };

        extern _schedule_st_ gschedule_st;

        struct _main_co_st_ {
#ifdef M_PLATFORM_WIN
            LPVOID _ctx = 0;
#else
            ucontext_t _ctx;
            //char _stack[M_PUBLIC_STACK_SIZE];
#endif
            // current coroutine no
            int64_t _curno = -1;

            ~_main_co_st_();
        };

        extern thread_local _main_co_st_ gmainco;

        struct _memory_st_ {
            int _mem = 0;
#ifndef M_PLATFORM_WIN
            std::mutex _mu;
#endif
            void add(size_t s);
            void dec(size_t s);
        };

        extern _memory_st_ gmem;

#ifdef M_PLATFORM_WIN
        extern thread_local std::atomic_int gwincocount;
        extern thread_local std::atomic_int gworkid;
#endif

    }
}
