/*----------------------------------------------------------------
// Copyright 2021
// All rights reserved.
//
// author: 404558965@qq.com (xiaoquanjie)
// github: https://github.com/xiaoquanjie/cgo
// Created by xiaoqj on 2023/11/04
//----------------------------------------------------------------*/

#include "thread.h"
#include "task.h"
#include "coroutine.h"
#include <functional>

namespace cgo {
    namespace coroutine {
        _thread_st_::_thread_st_() {

        }

        _thread_st_::~_thread_st_() {
            this->stop();
            if (_thr) {
                delete _thr;
            }
        }

        void _thread_st_::start() {
            if (_stop || _thr) {
                return;
            }

            _thr = new std::thread(std::bind(&_thread_st_::on_run, this));
        }

        // thread-unsafety
        void _thread_st_::stop() {
            if (!_stop) {
                _stop = true;
                _thr->join();
            }
        }

        bool _thread_st_::idle() const {
            return _idle;
        }

        bool _thread_st_::is_stop() const {
            return _stop;
        }

        void _thread_st_::on_run() {
            int wait_time = 3;
            while (!_stop) {
                auto wait_task = gwaittask.pop(wait_time);
                if (wait_task) {
                    _idle = false;
                    run(wait_task->_routine);
                    _idle = true;
                }

                auto co_task = gcotask.pop(wait_time);
                if (co_task) {
                    _idle = false;
                    resume(co_task->_co_id);
                    _idle = true;
                }

//                if (idle_add()) {
//                    _stop = true;
//                }
            }
            _idle = false;
        }

        bool _thread_st_::idle_add() {
            bool no_co = false;
#ifdef M_PLATFORM_WIN
            no_co = num_in_thread() == 0;
#else
            no_co = num() == 0;
#endif
            if (no_co) {
                auto now = std::chrono::steady_clock::now();
                if (_idle_point == time_point()) {
                    _idle_point = now;
                    return false;
                }
                auto diff = std::chrono::duration_cast<std::chrono::seconds>(now - _idle_point);
                if (diff > std::chrono::seconds(60)) {
                    return true;
                }
                return false;
            } else {
                _idle_point = time_point();
                return false;
            }
        }
    }
}
