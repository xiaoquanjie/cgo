/*----------------------------------------------------------------
// Copyright 2021
// All rights reserved.
//
// author: 404558965@qq.com (xiaoquanjie)
// github: https://github.com/xiaoquanjie/cgo
// Created by xiaoqj on 2023/11/09

调度策略：
向队列投递协程任务
        如果是在主协程中，则放入全局对列
        如果是在子协程中
                如果本地队列未满，则放入本地队列
                否则放入全局对列

获取队列协程任务
        如果本地队列有任务，则获取任务运行
        如果本地队列为空
                如果全局队列中有任务，则偷取协程任务
                如果其他对列有作务，则偷取协程任务

触发新的调度线程
        如果全局对列中有堆积，则启动新线程进行调度
        如果本地对列中有堆积，则启动新线程进行调度

从其他队列中偷的时候，应该偷多少？

//----------------------------------------------------------------*/

#pragma once

#include "../common/macro.h"
#include "../common/time_pool.h"
#include "../common/print.h"
#include "../common/concurrentqueue.h"
#include "../common/work_steal_queue.hpp"
#include <thread>
#include <condition_variable>
#include <vector>

namespace cgo {
    namespace scheduler {
        using task_type = std::function<void()>;
        using concurrent_task_queue_type = moodycamel::ConcurrentQueue<task_type>;
        using wsq_task_queue_type = WorkStealingQueue<task_type*>;
        struct _scheduler_st_;

        struct _schedule_base_queue_st_ {
            using time_point = std::chrono::time_point<std::chrono::steady_clock>;
            time_point _clock;

            void record();
            void unrecord();
            bool delay();

            virtual ~_schedule_base_queue_st_() {}
            virtual std::size_t size() = 0;
            virtual void enqueue(const task_type& f) = 0;
            virtual void enqueue(task_type* f, bool&) = 0;
            virtual bool try_dequeue(task_type& f) = 0;
            virtual void steal(int32_t count, _schedule_base_queue_st_* to) = 0;
        };

        struct _schedule_local_queue_st_ : public _schedule_base_queue_st_ {
        private:
            wsq_task_queue_type* _queue;
        public:
            _schedule_local_queue_st_();
            ~_schedule_local_queue_st_();
            std::size_t size() override;
            void enqueue(const task_type& f) override;
            void enqueue(task_type* f, bool&) override;
            bool try_dequeue(task_type& f) override;
            void steal(int32_t count, _schedule_base_queue_st_* to) override;

            _schedule_local_queue_st_(const _schedule_local_queue_st_&) = delete;
            _schedule_local_queue_st_& operator=(const _schedule_local_queue_st_&) = delete;
        };

        struct _schedule_global_queue_st_ : public _schedule_base_queue_st_ {
        private:
            concurrent_task_queue_type* _queue;
        public:
            _schedule_global_queue_st_();
            ~_schedule_global_queue_st_();
            std::size_t size() override;
            void enqueue(const task_type& f) override;
            void enqueue(task_type* f, bool&) override;
            bool try_dequeue(task_type& f) override;
            void steal(int32_t count, _schedule_base_queue_st_* to) override;
        };

        struct _schedule_thread_st_ {
            int _work_id = 0;
            std::thread* _thr = 0;
            _scheduler_st_* _scheduler = 0;
            std::atomic<_schedule_base_queue_st_*> _local_task;
            std::atomic_uint64_t _task_op_cnt = 0;
            _schedule_thread_st_() {
                _local_task.store(0);
            }
            ~_schedule_thread_st_();
            _schedule_thread_st_(const _schedule_thread_st_&) = delete;
            _schedule_thread_st_& operator=(const _schedule_thread_st_&) = delete;
        };

        using schedule_thread_st_type = std::shared_ptr<_schedule_thread_st_>;
        using dead_thread_queue_type = slist<schedule_thread_st_type>;

        struct _scheduler_st_ {
            _schedule_global_queue_st_* _global_tasks;
            dead_thread_queue_type _dead_threads;
            std::vector<schedule_thread_st_type> _work_threads;
            int generate_work_id = 1;
            std::mutex _thread_mu;

            std::atomic_bool _stop = false;
            std::atomic_int _max_thr_cnt = 0;   // max thread count
            std::atomic_int _core_thr_cnt = 1;  // core thread count
            std::atomic_int _idle_thr_cnt = 0;  // idle thread count
            std::atomic_int _thr_cnt = 0;       // current thread count
            std::atomic_uint64_t _task_op_cnt = 0;

            async_time_pool _time_pool;
            std::atomic_flag _time_pool_flag;
            std::vector<task_type> _loops;
            std::atomic_flag _loops_flag;

            _scheduler_st_();

            ~_scheduler_st_();

            void stop();

            bool run();

            bool can_start(bool force);

            void start_thread(bool force);

            void start_thread(bool force, _schedule_base_queue_st_* newq);

            bool try_dead_thread();

            void dead_thread(_schedule_thread_st_* st);

            void steal_task(_schedule_base_queue_st_* from, _schedule_base_queue_st_* to, int32_t count);

            void steal_task(_schedule_thread_st_* st);

            void print_debug_info();
        };

        extern _schedule_base_queue_st_* gglobal_task_queue;
        extern thread_local _schedule_base_queue_st_* volatile glocal_task_queue;

        _scheduler_st_& scheduler_inst();
        void add_global_task(task_type&& f);
        void add_local_task(task_type&& f, bool nosteal);
        uint64_t cur_coid();
        void schedule_task(const task_type& routine, int stack, const char* file, int line);
        void schedule_wait(int wait_mil);
        void schedule_yield(void*& data, const task_type& after);
        void schedule_yield(void*& data);
        void schedule_yield();
        void schedule_co(uint64_t co_id, void*);
        void set_cgo_procs(int cnt);
        void set_cgo_core(int cnt);
        void cgo_add_loop(const task_type& f);
        void cgo_stop();
        void print_debug_info();
        // try to start a new thread
        void trigger_new_thread(bool force);
        void thread_func(int work_id, _schedule_thread_st_* st);
    }
}