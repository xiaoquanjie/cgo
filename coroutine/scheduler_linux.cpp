/*----------------------------------------------------------------
// Copyright 2021
// All rights reserved.
//
// author: 404558965@qq.com (xiaoquanjie)
// github: https://github.com/xiaoquanjie/cgo
// Created by xiaoqj on 2023/11/09
//----------------------------------------------------------------*/


#include "macro.h"

#ifndef M_PLATFORM_WIN

#include "scheduler.h"

namespace cgo {
    namespace scheduler {
        _scheduler_st_ gscheduler;

        void working_thread(int work_id) {
            std::chrono::time_point<std::chrono::steady_clock> last_idle_time;
            std::chrono::seconds idle_add_time(0);

            auto& scheduler = gscheduler;
            M_CO_DEBUG_PRINT("start cgo working thread:%d\n", work_id);

            while (true) {
                // grab the run right of time pool
                std::chrono::milliseconds wait_t(10);
                if (!scheduler._time_pool_flag.test_and_set()) {
                    scheduler._time_pool.update();
                    scheduler._time_pool_flag.clear();
                } else {
                    wait_t = std::chrono::milliseconds(2000);
                }

                std::function<void()> task;
                {
                    std::unique_lock<std::mutex> task_lock(scheduler._task_mu);
                    if (scheduler._tasks.empty()) {
                        scheduler._task_cond.wait_for(task_lock, wait_t);
                    }

                    if (scheduler._stop) {
                        break;
                    }

                    if (!scheduler._tasks.empty()) {
                        task = scheduler._tasks.front();
                        scheduler._tasks.pop();
                    }
                }

                if (task) {
                    idle_add_time = std::chrono::seconds(0);
                    scheduler._idle_thr_cnt--;
                    task();
                    scheduler._idle_thr_cnt++;
                } else {
                    auto now = std::chrono::steady_clock::now();
                    if (idle_add_time == std::chrono::seconds(0)) {
                        idle_add_time = std::chrono::seconds(1);
                    } else {
                        idle_add_time += std::chrono::duration_cast<std::chrono::seconds>(now - last_idle_time);
                    }
                    last_idle_time = now;

                    if (idle_add_time >= std::chrono::seconds(M_CO_IDLE_TIME)) {
                        std::unique_lock<std::mutex> lock(scheduler._thread_mu);
                        if (scheduler._threads.size() > scheduler._core_thr_cnt) {
                            scheduler._threads[work_id].detach();
                            scheduler._threads.erase(work_id);
                            break;
                        }
                    }
                }
            }

            scheduler._idle_thr_cnt--;
            M_CO_DEBUG_PRINT("quit cgo working thread:%d\n", work_id);
        }

        // thread-safety
        void schedule_co(int64_t co_id, void* data) {
            std::function<void()> f = std::bind(coroutine::resume, co_id, data);
            add_global_task(std::move(f));
            start_thread();
        }

        void schedule_wait(int wait_mil) {
            auto co_id = coroutine::curid();
            if (co_id == M_INVALID_COROUTINE_ID) {
                return;
            }

            std::function<void()> f = std::bind(scheduler::schedule_co, co_id, (void*)NULL);
            gscheduler._time_pool.async_add_timer(wait_mil, f);
            start_thread();

            coroutine::yield();
        }
    }
}

#endif
