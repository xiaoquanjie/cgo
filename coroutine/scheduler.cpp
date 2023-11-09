/*----------------------------------------------------------------
// Copyright 2021
// All rights reserved.
//
// author: 404558965@qq.com (xiaoquanjie)
// github: https://github.com/xiaoquanjie/cgo
// Created by xiaoqj on 2023/11/04
//----------------------------------------------------------------*/

#include "scheduler.h"
#include "structure.h"
#include "macro.h"
#include <memory>

namespace cgo {
    namespace scheduler {
        _base_scheduler_st_::_base_scheduler_st_() {
            _max_thr_cnt = std::thread::hardware_concurrency() + 1;
        }

        _base_scheduler_st_::~_base_scheduler_st_() {
            this->stop();
        }

        void _base_scheduler_st_::stop() {
            std::unique_lock<std::mutex> lock(this->_thread_mu);
            this->_stop = true;
            for (auto& kv : this->_threads) {
                kv.second.join();
            }
            this->_threads.clear();
        }

        void set_cgo_procs(int cnt) {
            gscheduler._max_thr_cnt = cnt;
        }

        void set_core_pool(int cnt) {
            gscheduler._core_thr_cnt = cnt;
        }

        void stop() {
            gscheduler.stop();
        }

        void add_global_task(std::function<void()>&& f) {
            std::unique_lock<std::mutex> lock(gscheduler._task_mu);
            gscheduler._tasks.push(f);
            gscheduler._task_cond.notify_one();
        }

        void start_thread() {
            auto& scheduler = gscheduler;
            if ((int)scheduler._tasks.size() <= scheduler._idle_thr_cnt) {
                return;
            }

            std::unique_lock<std::mutex> lock(scheduler._thread_mu);
            if ((int)scheduler._threads.size() >= scheduler._max_thr_cnt) {
                return;
            }

            int task_cnt = (int)scheduler._tasks.size();
            int count = task_cnt - (int)scheduler._idle_thr_cnt;
            scheduler._idle_thr_cnt += count;

            while (count > 0) {
                auto work_id = scheduler.generate_work_id++;
                std::function<void()> work = std::bind(working_thread, work_id);
                scheduler._threads[work_id] = std::move(std::thread(work));
                count--;

                //M_CO_DEBUG_PRINT("start....\n");
            }
        }

        // thread-safety
        void schedule_task(const std::function<void()>& routine, int stack, const char* file, int line) {
            std::function<void()> f = std::bind(coroutine::run, routine, stack, file, line);
            add_global_task(std::move(f));
            start_thread();
        }
    }
}
