/*----------------------------------------------------------------
// Copyright 2021
// All rights reserved.
//
// author: 404558965@qq.com (xiaoquanjie)
// github: https://github.com/xiaoquanjie/cgo
// Created by xiaoqj on 2023/11/09
//----------------------------------------------------------------*/

#pragma once

#include "macro.h"
#include "../common/time_pool.h"
#include "../common/print.h"
#include "../common/concurrentqueue.h"
#include "../common/work_steal_queue.hpp"
#include <thread>
#include <condition_variable>
#include <unordered_map>

namespace cgo {
    namespace coroutine {
        void run(std::function<void()> routine, int stack, const char* file, int line);
        void resume(int64_t co_id, void* data);
        int64_t curid();
        int64_t real_curid();
        void yield();
        void yield(void** data);
#ifdef M_PLATFORM_WIN
        void decode_coid(int64_t co_id, int32_t& work_id, int64_t& real_id);
        int num_in_thread();
#endif
    }

    namespace scheduler {
        using task_type = std::function<void()>;
        using concurrent_task_queue_type = moodycamel::ConcurrentQueue<task_type>;
        using wsq_task_queue_type = WorkStealingQueue<task_type*>;

        struct _schedule_base_queue_st_ {
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
            _schedule_base_queue_st_* _local_task = 0;
            _schedule_base_queue_st_* _nosteal_local_task = 0;
            ~_schedule_thread_st_();
            _schedule_thread_st_(const _schedule_thread_st_&) = delete;
            _schedule_thread_st_& operator=(const _schedule_thread_st_&) = delete;
        };

        struct _local_task_st_ {
            slist<std::function<void()>> _tasks;
            std::mutex _task_mu;
        };

        struct _scheduler_st_ {
            async_time_pool _time_pool;
            std::atomic_flag _time_pool_flag;

            _schedule_global_queue_st_ _global_tasks;
            std::unordered_map<int, _schedule_thread_st_*> _work_threads;

            slist<std::function<void()>> _tasks;
            std::mutex _task_mu;
            std::condition_variable _task_cond;

            std::atomic_bool _stop = false;
            std::atomic_int _max_thr_cnt = 0;
            std::atomic_int _core_thr_cnt = 1;
            std::atomic_int _idle_thr_cnt = 0;

            int generate_work_id = 1;
            std::unordered_map<int, std::thread> _threads;
            std::mutex _thread_mu;

            std::unordered_map<int, std::shared_ptr<_local_task_st_>> _thr_tasks;

            _scheduler_st_();

            ~_scheduler_st_();

            void stop();
        };

        extern _scheduler_st_ gscheduler;
        extern _schedule_base_queue_st_* global_task_queue;
        extern thread_local _schedule_base_queue_st_* glocal_task_queue;
        extern thread_local _schedule_base_queue_st_* gnosteal_local_task_queue; // for windows

        void add_global_task(std::function<void()>&& f);
        void add_local_task(std::function<void()>&& f);
        void working_thread(int work_id);
        void schedule_task(const std::function<void()>& routine, int stack, const char* file, int line);
        void schedule_wait(int wait_mil);
        void schedule_co(int64_t co_id, void*);
        void set_cgo_procs(int cnt);
        void set_core_pool(int cnt);
        void stop();
        void start_thread();
    }
}