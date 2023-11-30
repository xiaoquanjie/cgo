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
#include <memory>

namespace cgo {
    namespace scheduler {
        _schedule_local_queue_st_::_schedule_local_queue_st_() {
            _queue = new wsq_task_queue_type(M_MAX_LOCAL_TASK_QUEUE);
        }

        _schedule_local_queue_st_::~_schedule_local_queue_st_() {
            while (true) {
                auto opt = _queue->pop();
                if (opt == std::nullopt) {
                    break;
                }
                delete opt.value();
            }
            delete _queue;
        }

        std::size_t _schedule_local_queue_st_::size() {
            return _queue->size();
        }

        void _schedule_local_queue_st_::enqueue(const std::function<void()>& f) {
            task_type* task = new task_type(f);
            _queue->push(task);
        }

        void _schedule_local_queue_st_::enqueue(task_type* f, bool& forward) {
            _queue->push(f);
            forward = true;
        }

        bool _schedule_local_queue_st_::try_dequeue(std::function<void()>& f) {
            auto opt = _queue->pop();
            if (opt == std::nullopt) {
                return false;
            }

            f = *opt.value();
            delete opt.value();
            return true;
        }

        void _schedule_local_queue_st_::steal(int32_t count, _schedule_base_queue_st_* to) {
            while (count > 0) {
                count--;
                auto opt = _queue->steal();
                if (opt == std::nullopt) {
                    break;
                }

                bool forward = false;
                to->enqueue(opt.value(), forward);
                if (!forward) {
                    delete opt.value();
                }
            }
        }

        _schedule_global_queue_st_::_schedule_global_queue_st_() {
            _queue = new concurrent_task_queue_type;
        }

        _schedule_global_queue_st_::~_schedule_global_queue_st_() {
            delete _queue;
        }

        std::size_t _schedule_global_queue_st_::size() {
            return _queue->size_approx();
        }

        void _schedule_global_queue_st_::enqueue(const task_type& f) {
            _queue->enqueue(f);
        }

        void _schedule_global_queue_st_::enqueue(task_type* f, bool& forward) {
            _queue->enqueue(*f);
            forward = false;
        }

        bool _schedule_global_queue_st_::try_dequeue(task_type& f) {
            return _queue->try_dequeue(f);
        }

        void _schedule_global_queue_st_::steal(int32_t count, _schedule_base_queue_st_* to) {
            while (count > 0) {
                count--;
                task_type f;
                if (!_queue->try_dequeue(f)) {
                    break;
                }
                to->enqueue(f);
            }
        }

        _schedule_thread_st_::~_schedule_thread_st_() {
            if (_thr) {
                delete _thr;
            }
        }

        _scheduler_st_::_scheduler_st_() {
            _max_thr_cnt = (int)(std::thread::hardware_concurrency() * M_MAX_PROCS_FACTOR);
            _core_thr_cnt = (int)(_max_thr_cnt * M_CORE_POOL_FACTOR);
        }

        _scheduler_st_::~_scheduler_st_() {
            this->stop();
        }

        void _scheduler_st_::stop() {
            std::unique_lock<std::mutex> lock(this->_thread_mu);
            this->_stop = true;
            for (auto& kv : this->_threads) {
                kv.second.join();
            }
            this->_threads.clear();
        }

        _scheduler_st_ gscheduler;
        _schedule_base_queue_st_* global_task_queue = &gscheduler._global_tasks;
        thread_local _schedule_base_queue_st_* glocal_task_queue = global_task_queue;
        thread_local _schedule_base_queue_st_* gnosteal_local_task_queue = nullptr;

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
            if (glocal_task_queue == global_task_queue) {
                glocal_task_queue->enqueue(f);
            } else {
                if (glocal_task_queue->size() >= M_MAX_LOCAL_TASK_QUEUE) {
                    global_task_queue->enqueue(f);
                } else {
                    glocal_task_queue->enqueue(f);
                }
            }
        }

        void add_local_task(std::function<void()>&& f) {
            if (glocal_task_queue == global_task_queue) {
                assert(false);
            } else {
                glocal_task_queue->enqueue(f);
            }
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
