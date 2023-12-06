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
        void thread_func(int work_id, _schedule_thread_st_* st) {
            M_CO_DEBUG_PRINT("start cgo working thread:%d\n", work_id);

            st->_local_task.store(new _schedule_local_queue_st_);
            glocal_task_queue = st->_local_task;

            int64_t idle_beg_time = 0;
            bool idle_quit = false;
            int idles = 0;

            for (;;) {
                st->_scheduler->run();

                // run local task
                task_type task;
                while (glocal_task_queue->try_dequeue(task)) {
                    idle_beg_time = 0;
                    idles = 0;
                    st->_scheduler->_idle_thr_cnt--;
                    st->_scheduler->_task_op_cnt++;
                    st->_task_op_cnt++;
                    task();
                    st->_scheduler->_idle_thr_cnt++;
                    if (st->_scheduler->_stop) {
                        break;
                    }
                }

                if (glocal_task_queue->size() == 0) {
                    // steal from the other queue
                    st->_scheduler->steal_task(st);
                    if (glocal_task_queue->size() == 0) {
                        // idle
                        idles++;
                        if (idle_beg_time == 0) {
                            idle_beg_time =
                                    std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
                        }
                        if (idles >= 500) {
                            std::this_thread::sleep_for(std::chrono::milliseconds(10));
                        }
                    }
                }

                if (st->_scheduler->_stop) {
                    break;
                }

                if (idle_beg_time != 0) {
                    auto now =
                            std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
                    if (now - idle_beg_time >= M_CO_IDLE_TIME) {
                        if (st->_scheduler->try_dead_thread()) {
                            idle_quit = true;
                            M_CO_DEBUG_PRINT("idle thread:%d\n", work_id);
                            break;
                        }
                    }
                }
            }

            glocal_task_queue = nullptr;

            if (idle_quit) {
                st->_scheduler->dead_thread(st);
            }
            M_CO_DEBUG_PRINT("quit cgo working thread:%d\n", work_id);
        }

        // thread-safety
        void schedule_co(uint64_t co_id, void* data) {
            std::function<void()> f = std::bind(coroutine::resume, co_id, data);
            add_global_task(std::move(f));
        }

        void schedule_wait(int wait_mil) {
            assert(wait_mil <= M_MAX_CO_WAIT_TIME * 1000);
            auto co_id = coroutine::curid();
            if (co_id == M_INVALID_COROUTINE_ID) {
                return;
            }

            std::function<void()> timer_func = [co_id]() {
                std::function<void()> task = std::bind(coroutine::resume, co_id, nullptr);
                add_local_task(std::move(task), false);
            };

            gscheduler._time_pool.async_add_timer(wait_mil, std::move(timer_func));
            schedule_yield(0);
        }
    }
}

#endif
