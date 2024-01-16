/*----------------------------------------------------------------
// Copyright 2021
// All rights reserved.
//
// author: 404558965@qq.com (xiaoquanjie)
// github: https://github.com/xiaoquanjie/cgo
// Created by xiaoqj on 2023/10/31.
//----------------------------------------------------------------*/

#pragma once

enum CoroutineStatus {
    COROUTINE_NONE = 0,
    COROUTINE_READY = 1,
    COROUTINE_RUNNING = 2,
    COROUTINE_SUSPEND = 3,
    COROUTINE_DEAD = 4,
};

// invalid coroutine id
#undef M_INVALID_COROUTINE_ID
#define M_INVALID_COROUTINE_ID (uint64_t)(-1)

#undef M_PRIVATE_STACK_SIZE
#define M_PRIVATE_STACK_SIZE 1024*64

#undef GROWUP_COROUTINE
#define GROWUP_COROUTINE (1024*100)

#undef M_CO_IDLE_TIME
#define M_CO_IDLE_TIME (2*60)

#undef M_CORE_POOL_FACTOR
#define M_CORE_POOL_FACTOR (0.3)

#undef M_MAX_PROCS_FACTOR
#define M_MAX_PROCS_FACTOR (1)

#undef M_MAX_LOCAL_TASK_QUEUE
#define M_MAX_LOCAL_TASK_QUEUE (256)

// microseconds
#undef M_QUEUE_DELAY_TIME
#define M_QUEUE_DELAY_TIME (5000)

#undef M_MAX_CO_WAIT_TIME
#define M_MAX_CO_WAIT_TIME (1800)

#undef M_CO_POOL_SIZE
#define M_CO_POOL_SIZE (1000)

