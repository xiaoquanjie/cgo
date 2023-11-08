/*----------------------------------------------------------------
// Copyright 2021
// All rights reserved.
//
// author: 404558965@qq.com (xiaoquanjie)
// github: https://github.com/xiaoquanjie/cgo
// Created by xiaoqj on 2023/11/04
//----------------------------------------------------------------*/

#include "../common/time_pool.h"
#include "../common/print.h"
#include "macro.h"
#include <memory>
#include <unordered_map>
#include <thread>
#include <condition_variable>

namespace cgo {
    namespace coroutine {
        void run(std::function<void()> routine, const char* file, int line);
        void resume(int64_t co_id);
        int64_t curid();
        void yield();
    }

    namespace scheduler {
        struct _scheduler_st_ {
            async_time_pool _time_pool;
            std::atomic_flag _time_pool_flag;

            slist<std::function<void()>> _tasks;
            int generate_work_id = 1;
            std::unordered_map<int, std::thread> _threads;
            std::mutex _thread_mu;

            std::mutex _task_mu;
            std::condition_variable _task_cond;

            std::atomic_bool _stop = false;
            std::atomic_int _max_thr_cnt = 0;
            std::atomic_int _core_thr_cnt = 0;
            std::atomic_int _idle_thr_cnt = 0;

            _scheduler_st_() {
                set_thr();
            }

            ~_scheduler_st_() {
                stop();
            }

            void set_thr(int cnt = 0) {
                if (cnt == 0) {
                    cnt = std::thread::hardware_concurrency() + 1;
                }

                _max_thr_cnt = cnt;
                _core_thr_cnt = (cnt / 2);
                if (_core_thr_cnt == 0) {
                    _core_thr_cnt = 1;
                }
            }

            void stop() {
                std::unique_lock<std::mutex> lock(this->_thread_mu);
                this->_stop = true;
                for (auto& kv : this->_threads) {
                    kv.second.join();
                }
                this->_threads.clear();
            }
        };

        _scheduler_st_ gscheduler;

        void set_max_procs(int cnt) {
            gscheduler.set_thr(cnt);
        }

        void stop() {
            gscheduler.stop();
        }

        void add_task(std::function<void()>&& f) {
            std::unique_lock<std::mutex> lock(gscheduler._task_mu);
            gscheduler._tasks.push(f);
            gscheduler._task_cond.notify_one();
        }

        void working_thread(int work_id) {
            int idle_time = 0;
            auto& scheduler = gscheduler;
            scheduler._idle_thr_cnt++;
            //M_CO_DEBUG_PRINT("start working thread:%d\n", work_id);

            while (true) {
                std::chrono::milliseconds wait_t(10);

                // grab the run right of time pool
                if (!scheduler._time_pool_flag.test_and_set()) {
                    scheduler._time_pool.update();
                    scheduler._time_pool_flag.clear();
                } else {
                    wait_t = std::chrono::milliseconds(10);
                }

                std::function<void()> task;

                {
                    std::unique_lock<std::mutex> task_lock(scheduler._task_mu);
                    scheduler._task_cond.wait_for(task_lock, wait_t);

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
                } else {
                    idle_time += wait_t.count();
                    if (idle_time >= 60 * 1000) {
                        // idle over one minute
                        idle_time = 0;
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
            M_CO_DEBUG_PRINT("quit working thread:%d\n", work_id);
        }

        void start_thread() {
            auto& scheduler = gscheduler;
            if (scheduler._tasks.size() <= scheduler._idle_thr_cnt) {
                return;
            }

            std::unique_lock<std::mutex> lock(scheduler._thread_mu);
            if (scheduler._threads.size() >= scheduler._max_thr_cnt) {
                return;
            }

            int task_cnt = scheduler._tasks.size();
            int count = task_cnt - (int)scheduler._idle_thr_cnt;

            while (task_cnt > 0) {
                auto work_id = scheduler.generate_work_id++;
                std::function<void()> work = std::bind(working_thread, work_id);
                scheduler._threads[work_id] = std::move(std::thread(work));
                task_cnt--;
            }
        }

        inline void schedule(std::function<void()>&& f) {
            add_task(std::move(f));
            start_thread();
        }

        // thread-safety
        void schedule_task(const char* file, int line, std::function<void()> routine) {
            std::function<void()> f = std::bind(coroutine::run, routine, file, line);
            schedule(std::move(f));
        }

        // thread-safety
        void schedule_co(int64_t co_id) {
            std::function<void()> f = std::bind(coroutine::resume, co_id);
            schedule(std::move(f));
        }

        void schedule_wait(int wait_mil) {
            auto co_id = coroutine::curid();
            if (co_id == M_INVALID_COROUTINE_ID) {
                return;
            }

            std::function<void()> f = std::bind(scheduler::schedule_co, co_id);
            gscheduler._time_pool.async_add_timer(wait_mil, f);
            start_thread();

            coroutine::yield();
        }
    }
}
