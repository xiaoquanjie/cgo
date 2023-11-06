/*----------------------------------------------------------------
// Copyright 2021
// All rights reserved.
//
// author: 404558965@qq.com (xiaoquanjie)
// github: https://github.com/xiaoquanjie/cgo
// Created by xiaoqj on 2023/11/04
//----------------------------------------------------------------*/

#include "task.h"
#include "thread.h"
#include <vector>
#include <memory>

namespace cgo {
    namespace coroutine {
        void on_exit();

        using thread_st_ptr = std::shared_ptr<_thread_st_>;

        struct _scheduler_st_ {
            int _max_count = 0;
            std::vector<thread_st_ptr> _thrs;
            std::mutex _mu;

            _scheduler_st_() {
                _max_count = std::thread::hardware_concurrency();
                std::atexit(on_exit);
            }

            // thread-safety
            void schedule_task() {
                try_new_thread();
            }

            void schedule_co() {
                try_new_thread();
            }

            void exit() {
                for (auto thr : _thrs) {
                    thr->stop();
                }
                _thrs.clear();
            }

        private:
            void try_new_thread() {
                if (_thrs.size() == _max_count) {
                    return;
                }

                std::unique_lock<std::mutex> lock(_mu);
                if (_thrs.size() == _max_count) {
                    return;
                }

                while (_thrs.size() < _max_count) {
                    auto t = std::make_shared<_thread_st_>();
                    t->start();
                    _thrs.push_back(t);
                }
            }

            bool has_idle_thr(std::vector<thread_st_ptr>& tmp_thrs) {
                for (auto thr : tmp_thrs) {
                    if (thr->is_stop() == false && thr->idle()) {
                        return true;
                    }
                }
                return false;
            }
        };

        _scheduler_st_ gscheduler;

        void on_exit() {
            gscheduler.exit();
        }

        // thread-safety
        void schedule_task(const char* file, int line, std::function<void()> routine) {
            gwaittask.push(file, line, routine);
            gscheduler.schedule_task();
        }

        // thread-safety
        void schedule_co(int64_t co_id) {
            gcotask.push(co_id);
            gscheduler.schedule_co();
        }
    }
}

