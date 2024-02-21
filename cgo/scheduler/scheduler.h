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

#include <functional>
#include <stdint.h>

namespace cgo {
    namespace scheduler {
        using routine_fn = std::function<void()>;
        struct _scheduler_st_;
        struct _schedule_thread_st_;

        _scheduler_st_& scheduler_inst();
        uint64_t cur_coid();
        void schedule_task(const routine_fn& routine, int stack, const char* file, int line);
        void schedule_wait(int wait_mil);
        void schedule_yield();
        void schedule_wait_signal();
        void schedule_wait_signal(void*& data);
        void schedule_post_signal(uint64_t co_id, void* data);
        void schedule_co(uint64_t co_id);
        void set_cgo_procs(int cnt);
        void set_cgo_core(int cnt);
        void cgo_add_loop(const routine_fn& f);
        void cgo_stop();
        void co_hook(bool enable);
        bool co_hook();
        void print_debug_info(bool enable);
        void cgo_default_stack(int stack);
    }
}