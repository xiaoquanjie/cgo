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
#include <algorithm>
#include <string>

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
            assert(_queue->enqueue(f));
        }

        void _schedule_global_queue_st_::enqueue(task_type* f, bool& forward) {
            assert(_queue->enqueue(*f));
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

        //////////////////////////////////////////////////////////////

        _co_pool_st_::_co_pool_item_st_* _co_pool_st_::create_item(const task_type& routine, const char* file, int line) {
            _co_pool_item_st_* item = 0;
            if (!_pool.try_dequeue(item)) {
                item = new _co_pool_item_st_;
                task_type f = std::bind(co_pool_func, item);
                item->_co_id = coroutine::create(f, 0, 0, 0);
                item->_routine = new task_type;
                item->_co_pool = this;
            }
            *item->_routine = routine;
            item->_file = file;
            item->_line = line;
            return item;
        }

        bool _co_pool_st_::recycle_item(_co_pool_st_::_co_pool_item_st_* item) {
            if (_pool.size_approx() >= M_CO_POOL_SIZE) {
                delete item->_routine;
                delete item;
                return false;
            } else {
                _pool.enqueue(item);
                return true;
            }
        }

        //////////////////////////////////////////////////////////////

        _schedule_thread_st_::~_schedule_thread_st_() {
            if (_thr) {
                delete _thr;
            }

            _schedule_base_queue_st_* task = _local_task.load();
            if (task) {
                if (!_scheduler->_stop) {
                    if (task->size() > 0) {
                        assert(false);
                    }
                }
                delete task;
            }

#ifdef M_PLATFORM_WIN
            if (_nosteal_local_task) {
                if (!_scheduler->_stop) {
                    if (_nosteal_local_task->size() > 0) {
                        assert(false);
                    }
                }
                delete _nosteal_local_task;
            }
#endif
        }

        _scheduler_st_::_scheduler_st_() {
            _max_thr_cnt = (int)(std::thread::hardware_concurrency() * M_MAX_PROCS_FACTOR);
            _core_thr_cnt = (int)(_max_thr_cnt * M_CORE_POOL_FACTOR);

            std::atexit(cgo::scheduler::cgo_stop);
        }

        _scheduler_st_::~_scheduler_st_() {
            this->stop();
        }

        void _scheduler_st_::stop() {
            std::vector<std::shared_ptr<_schedule_thread_st_>> work_threads;
            {
                std::unique_lock<std::mutex> scoped_lock(_thread_mu);
                this->_stop = true;
                work_threads = _work_threads;
                this->_work_threads.clear();
                _thr_cnt = 0;
                _idle_thr_cnt = 0;
            }

            for (auto& thr : work_threads) {
                thr->_thr->join();
            }
            work_threads.clear();
        }

#ifndef M_PLATFORM_WIN
        bool _scheduler_st_::run() {
            if (!_time_pool_flag.test_and_set()) {
                _time_pool.update();
                _time_pool_flag.clear();

                if (_dead_threads.size()) {
                    std::unique_lock<std::mutex> scoped_lock(_thread_mu);
                    _dead_threads.clear();
                }
                return true;
            }
            return false;
        }
#endif

        void _scheduler_st_::start_thread() {
            auto task_size = _global_tasks.size();
            if (_thr_cnt >= _max_thr_cnt
                || (_idle_thr_cnt != 0 && task_size < M_MAX_LOCAL_TASK_QUEUE)) {
                return;
            }

            std::unique_lock<std::mutex> scoped_lock(_thread_mu);

            // double check
            task_size = _global_tasks.size();
            if (_thr_cnt >= _max_thr_cnt
                || (_idle_thr_cnt != 0 && task_size < M_MAX_LOCAL_TASK_QUEUE)) {
                return;
            }

            _idle_thr_cnt++;
            //M_CO_DEBUG_PRINT("idle_thr_cnt:%d\n", _idle_thr_cnt.load());
            _thr_cnt++;
            auto work_id = generate_work_id++;

            auto st = std::make_shared<_schedule_thread_st_>();
            st->_work_id = work_id;
            st->_scheduler = this;
            _work_threads.emplace_back(st);
            std::function<void()> work = std::bind(thread_func, work_id, st.get());
            st->_thr = new std::thread(work);
        }

        bool _scheduler_st_::try_dead_thread() {
            auto old = _thr_cnt.load(std::memory_order_relaxed);
            if (old <= _core_thr_cnt) {
                return false;
            }

            auto ret = _thr_cnt.compare_exchange_weak(old, old - 1);
            return ret;
        }

        void _scheduler_st_::dead_thread(_schedule_thread_st_* st) {
            _idle_thr_cnt--;
            _thr_cnt--;

            std::unique_lock<std::mutex> scoped_lock(_thread_mu);
            for (auto iter = _work_threads.begin(); iter != _work_threads.end(); ++iter) {
                if (iter->get() == st) {
                    st->_thr->detach();
                    _dead_threads.push(*iter);
                    _work_threads.erase(iter);
                    break;
                }
            }
        }

        void _scheduler_st_::steal_task(_schedule_thread_st_* st) {
            // steal from global first
            // max local task queue: M_MAX_LOCAL_TASK_QUEUE
            this->_global_tasks.steal(M_MAX_LOCAL_TASK_QUEUE, st->_local_task);

            _schedule_base_queue_st_* to_local_task = st->_local_task;
            if (_thr_cnt <= 1 || to_local_task->size() > 0) {
                // can't steal from self
                return;
            }

            // steal from other queue
            _thread_mu.lock();
            std::vector<std::shared_ptr<_schedule_thread_st_>> tmp_work_threads;
            tmp_work_threads = _work_threads;
            _thread_mu.unlock();

            if (tmp_work_threads.empty()) {
                return;
            }

            uint64_t full = tmp_work_threads.size();
            uint64_t rn = (uint64_t)(&tmp_work_threads);
            rn %= full;
            rn = (uint64_t)tmp_work_threads[rn].get();
            rn %= full;

            while (full > 0) {
                full--;
                auto i = (rn++) % tmp_work_threads.size();
                if (tmp_work_threads[i].get() == st) {
                    continue;
                }

                auto local_task = tmp_work_threads[i]->_local_task.load();
                if (!local_task) {
                    continue;
                }

                auto has = local_task->size();
                if (has <= 0) {
                    continue;
                }

                has = has % 2 != 0 ? (has / 2) + 1 : has / 2;
                int32_t need = M_MAX_LOCAL_TASK_QUEUE - (int32_t)to_local_task->size();
                if (need <= 0) {
                    continue;
                }

                // (std::min) for windows
                need = (std::min)(need, (int32_t)has);
                local_task->steal(need, to_local_task);
                if (to_local_task->size() > 0) {
                    break;
                }
            }
        }

        void _scheduler_st_::print_debug_info() {
            _thread_mu.lock();
            std::vector<std::shared_ptr<_schedule_thread_st_>> tmp_work_threads;
            tmp_work_threads = _work_threads;
            _thread_mu.unlock();

            std::string output = "queue info:\n";
            output += std::string("global queue count:") + std::to_string(this->_global_tasks.size());
            output += std::string(", global task operation count:") + std::to_string(this->_task_op_cnt);
            output += std::string("\n");
            for (auto thr : tmp_work_threads) {
                auto local_task = thr->_local_task.load();
                if (!local_task) {
                    continue;
                }
                output += std::string("work id:") + std::to_string(thr->_work_id) + std::string(" local queue count:") + std::to_string(local_task->size());
                output += std::string(", local task operation count:") + std::to_string(thr->_task_op_cnt);
                output += std::string("\n");
            }
            M_CO_DEBUG_PRINT("%s\n", output.c_str());
        }

        _scheduler_st_ gscheduler;
        _schedule_base_queue_st_* gglobal_task_queue = &gscheduler._global_tasks;
        thread_local _schedule_base_queue_st_* glocal_task_queue = gglobal_task_queue;
        thread_local _schedule_base_queue_st_* gnosteal_local_task_queue = nullptr;
        thread_local time_pool* glocal_time_pool = nullptr;

        void set_cgo_procs(int cnt) {
            if (cnt < 1) {
                cnt = 1;
            }
            gscheduler._max_thr_cnt = cnt;
        }

        void set_cgo_core(int cnt) {
            if (cnt < 1) {
                cnt = 1;
            }
            gscheduler._core_thr_cnt = cnt;
        }

        void cgo_stop() {
            M_CO_DEBUG_PRINT("cgo stop\n");
            gscheduler.stop();
        }

        void print_debug_info() {
            gscheduler.print_debug_info();
        }

        void co_pool_func(void* i) {
            _co_pool_st_::_co_pool_item_st_* item = (_co_pool_st_::_co_pool_item_st_*)i;
            assert(item != 0);
            assert(item->_routine || !(*item->_routine));

            while (true) {
                (*item->_routine)();
                if (item->_co_pool->recycle_item(item)) {
                    schedule_yield(0);
                    continue;
                }
                break;
            }
        }

        void add_global_task(task_type&& f) {
            if (glocal_task_queue == gglobal_task_queue) {
                glocal_task_queue->enqueue(f);
            } else {
                if (glocal_task_queue->size() >= M_MAX_LOCAL_TASK_QUEUE) {
                    gglobal_task_queue->enqueue(f);
                } else {
                    glocal_task_queue->enqueue(f);
                }
            }
            trigger_new_thread();
        }

        void add_local_task(task_type&& f, bool nosteal) {
            if (glocal_task_queue == gglobal_task_queue) {
                assert(false);
            } else {
                if (nosteal) {
                    gnosteal_local_task_queue->enqueue(f);
                } else {
                    glocal_task_queue->enqueue(f);
                    trigger_new_thread();
                }
            }
        }

        // thread-safety
        void schedule_task(const task_type& routine, int stack, const char* file, int line) {
            task_type f = std::bind(coroutine::run, routine, stack, file, line);
            add_global_task(std::move(f));
        }

        void trigger_new_thread() {
            gscheduler.start_thread();
        }

        void schedule_yield(void** data) {
            auto co_id = coroutine::curid();
            if (co_id == M_INVALID_COROUTINE_ID) {
                return;
            }

            coroutine::yield(data);
        }
    }
}
