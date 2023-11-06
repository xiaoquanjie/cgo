/*----------------------------------------------------------------
// Copyright 2021
// All rights reserved.
//
// author: 404558965@qq.com (xiaoquanjie)
// github: https://github.com/xiaoquanjie/cgo
// Created by xiaoqj on 2023/11/04
//----------------------------------------------------------------*/

#pragma once

#include "../common/slist.h"
#include "macro.h"
#include <functional>
#include <stdint.h>
#include <memory>
#include <mutex>
#include <condition_variable>

namespace cgo {
    namespace coroutine {
        struct _wait_task_st_ {
            const char* _file = 0;
            int _line = 0;
            std::function<void()> _routine;
        };

        using wait_task_ptr = std::shared_ptr<_wait_task_st_>;

        struct _co_task_st_ {
            int64_t _co_id = -1;
        };

        using co_task_ptr = std::shared_ptr<_co_task_st_>;

        class _wait_task_list_st_ {
        private:
            slist<wait_task_ptr> _list;
            std::mutex _mu;
            std::condition_variable _cond;

        public:
            void push(wait_task_ptr);

            void push(const char* file, int line, std::function<void()> routine);

            wait_task_ptr pop(int wait_time);
        };

        struct _co_task_list_st_ {
        private:
            slist<co_task_ptr> _list;
#ifndef M_PLATFORM_WIN
            std::mutex _mu;
            std::condition_variable _cond;
#endif
        public:
            void push(co_task_ptr);

            void push(int64_t co_id);

            co_task_ptr pop(int wait_time);
        };

        extern _wait_task_list_st_ gwaittask;

#ifdef M_PLATFORM_WIN
        extern thread_local _co_task_list_st_ gcotask;
#else
        extern _co_task_list_st_ gcotask;
#endif
    }
}