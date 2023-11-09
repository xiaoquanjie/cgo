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
        using local_task_list_ptr = std::shared_ptr<slist<std::function<void()>>>;
        thread_local local_task_list_ptr glocal_tasks;
        using local_task_mutext_ptr = std::shared_ptr<std::mutex>;
        thread_local local_task_mutext_ptr glocal_task_mu;

        _scheduler_st_ gscheduler;

        void working_thread(int work_id) {
            coroutine::gworkid = work_id;
            glocal_tasks = std::make_shared<slist<std::function<void()>>>();
            glocal_task_mu = std::make_shared<std::mutex>();

            int idle_time = 0;
            auto& scheduler = gscheduler;
            M_CO_DEBUG_PRINT("start working thread:%d\n", work_id);

            while (true) {
                // local task first
                slist<std::function<void()>> local_task;
                {
                    std::unique_lock<std::mutex> lock(*glocal_task_mu.get());
                    glocal_tasks->swap(local_task);
                }

                if (local_task.size()) {
                    scheduler._idle_thr_cnt--;
                    while (local_task.size()) {
                        auto task = local_task.front();
                        local_task.pop();
                        task();
                    }
                    scheduler._idle_thr_cnt++;
                }

                // grab the run right of time pool
                std::chrono::milliseconds wait_t(2000);
                if (!scheduler._time_pool_flag.test_and_set()) {
                    scheduler._time_pool.update();
                    scheduler._time_pool_flag.clear();
                    wait_t = std::chrono::milliseconds(10);
                }

                if (coroutine::num_in_thread() > 0) {
                    idle_time = 0;
                    wait_t = std::chrono::milliseconds(10);
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
                    idle_time = 0;
                    scheduler._idle_thr_cnt--;
                    task();
                    scheduler._idle_thr_cnt++;
                } 

				if (coroutine::num_in_thread() == 0) {
					idle_time += (int)wait_t.count();
					//M_CO_DEBUG_PRINT("wait:%d\n", wait_t.count());
					if (idle_time >= 60 * 1000) {
						// idle over one minute
						std::unique_lock<std::mutex> lock(scheduler._thread_mu);
						if ((int)scheduler._threads.size() > scheduler._core_thr_cnt) {
							scheduler._threads[work_id].detach();
							scheduler._threads.erase(work_id);
							break;
						}
						else {
							idle_time = 0;
						}
					}
				}
            }

            scheduler._idle_thr_cnt--;
            M_CO_DEBUG_PRINT("quit working thread:%d\n", work_id);
        }

        void schedule_co(int64_t co_id) {
            int32_t work_id = -1;
            int64_t real_co_id = -1;
            coroutine::decode_coid(co_id, work_id, real_co_id);
            assert(work_id != -1 && real_co_id != -1);

            //std::function<void()> f = std::bind(coroutine::resume, real_co_id);
        }

        void schedule_wait(int wait_mil) {
            auto co_id = coroutine::real_curid();
            if (co_id == M_INVALID_COROUTINE_ID) {
                return;
            }

            local_task_list_ptr local_task = glocal_tasks;
            local_task_mutext_ptr local_task_mu = glocal_task_mu;

            gscheduler._time_pool.async_add_timer(wait_mil, [co_id, local_task, local_task_mu]() {
                std::unique_lock<std::mutex> lock(*local_task_mu.get());
                local_task->push([co_id]() {
                    coroutine::resume(co_id);
                });
            });

            coroutine::yield();
        }
    }
}

#endif
