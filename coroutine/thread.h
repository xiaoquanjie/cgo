/*----------------------------------------------------------------
// Copyright 2021
// All rights reserved.
//
// author: 404558965@qq.com (xiaoquanjie)
// github: https://github.com/xiaoquanjie/cgo
// Created by xiaoqj on 2023/11/04
//----------------------------------------------------------------*/

#pragma once

#include <thread>
#include <atomic>

namespace cgo {
    namespace coroutine {
        class _thread_st_ {
            using time_point = std::chrono::time_point<std::chrono::steady_clock>;
        private:
            std::thread* _thr = 0;
            std::atomic_bool _stop = false;
            std::atomic_bool _idle = true;
            time_point _idle_point;

        public:
            _thread_st_();

            ~_thread_st_();

            void start();

            void stop();

            bool idle() const;

            bool is_stop() const;
        protected:
            void on_run();

            bool idle_add();
        private:
            _thread_st_(const _thread_st_&) = delete;
            _thread_st_& operator=(const _thread_st_&) = delete;
        };
    }
}