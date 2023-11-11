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
        struct _base_scheduler_st_ {
            async_time_pool _time_pool;
            std::atomic_flag _time_pool_flag;

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

            _base_scheduler_st_();

            ~_base_scheduler_st_();

            void stop();
        };
		
#ifdef M_PLATFORM_WIN
		struct _local_task_st_ {
			slist<std::function<void()>> _tasks;
			std::mutex _task_mu;
		};
		struct _scheduler_st_ : public _base_scheduler_st_ {
			std::unordered_map<int, std::shared_ptr<_local_task_st_>> _thr_tasks;
		};
#else
		struct _scheduler_st_ : public _base_scheduler_st_ {};
#endif

        extern _scheduler_st_ gscheduler;

        void add_global_task(std::function<void()>&& f);
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