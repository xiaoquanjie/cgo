/*----------------------------------------------------------------
// Copyright 2021
// All rights reserved.
//
// author: 404558965@qq.com (xiaoquanjie)
// github: https://github.com/xiaoquanjie/cgo
// Created by xiaoqj on 2023/11/09
//----------------------------------------------------------------*/

#include "macro.h"

#ifdef M_PLATFORM_WIN

#include "structure.h"
#include "scheduler.h"
#include <cassert>

namespace cgo {
    namespace coroutine {
        thread_local std::atomic_int gwincocount;
        thread_local std::atomic_int gworkid;
    }

    namespace scheduler {
        void thread_func(int work_id, _schedule_thread_st_* st) {
            M_CO_DEBUG_PRINT("start cgo working thread:%d\n", work_id);

            st->_local_task.store(new _schedule_local_queue_st_);
            glocal_task_queue = st->_local_task;

            st->_nosteal_local_task = new _schedule_global_queue_st_;
            gnosteal_local_task_queue = st->_nosteal_local_task;
            glocal_time_pool = &st->_time_pool;

            int64_t idle_beg_time = 0;
            bool idle_quit = false;
            int idles = 0;

            _schedule_base_queue_st_* local_queues[2] = { gnosteal_local_task_queue , glocal_task_queue };

            for (;;) {
                st->_time_pool.update();

                // run nosteal local task
                task_type task;
                for (auto& que : local_queues) {
                    while (que->try_dequeue(task)) {
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
            gnosteal_local_task_queue = nullptr;
            glocal_time_pool = nullptr;

            if (idle_quit) {
                st->_scheduler->dead_thread(st);
            }
            M_CO_DEBUG_PRINT("quit cgo working thread:%d\n", work_id);
        }

        void schedule_co(uint64_t co_id, void* data) {
            auto co = coroutine::_co_st_::get_co(co_id);
            assert(co != 0);
            _schedule_base_queue_st_* nosteal_que = (_schedule_base_queue_st_*)co->_lque;
            std::function<void()> task = std::bind(coroutine::resume, co_id, data);
            nosteal_que->enqueue(task);
        }

        void schedule_wait(int wait_mil) {
            assert(wait_mil <= M_MAX_CO_WAIT_TIME * 1000);
            auto co_id = coroutine::curid();
            if (co_id == M_INVALID_COROUTINE_ID) {
                return;
            }

            auto co = coroutine::_co_st_::get_co(co_id);
            assert(co != 0);
            co->_lque = (void*)gnosteal_local_task_queue;

            std::function<void()> timer_func = [co_id]() {
                std::function<void()> task = std::bind(coroutine::resume, co_id, nullptr);
                add_local_task(std::move(task), true);
            };

            glocal_time_pool->add_timer(wait_mil, std::move(timer_func));
            schedule_yield(0);
        }
    }
}

#endif
