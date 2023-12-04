/*----------------------------------------------------------------
// Copyright 2021
// All rights reserved.
//
// author: 404558965@qq.com (xiaoquanjie)
// github: https://github.com/xiaoquanjie/cgo
// Created by xiaoqj on 2023/10/31.
//----------------------------------------------------------------*/

#pragma once

#ifndef M_PLATFORM_WIN32
#if defined(_WIN32) || defined(__WIN32__) || defined(WIN32)
#define M_PLATFORM_WIN32 1
#endif
#endif

#ifndef M_PLATFORM_WIN
#if defined(M_PLATFORM_WIN32) || defined(WIN64) || defined(_WIN64_) || defined(_WIN64)
#define M_PLATFORM_WIN 1
#endif
#endif

#undef COROUTINE_READY
#define COROUTINE_READY   (1)

#undef COROUTINE_RUNNING
#define COROUTINE_RUNNING (2)

#undef COROUTINE_SUSPEND
#define COROUTINE_SUSPEND (3)

#undef COROUTINE_DEAD
#define COROUTINE_DEAD	  (4)

// invalid coroutine id
#undef M_INVALID_COROUTINE_ID
#define M_INVALID_COROUTINE_ID (uint64_t)(-1)

// for linux public stack
#undef M_PUBLIC_STACK_SIZE
#define M_PUBLIC_STACK_SIZE  1*1024*1024

// for windows stack
#undef M_PRIVATE_STACK_SIZE
#define M_PRIVATE_STACK_SIZE 1024*8

#undef GROWUP_COROUTINE
#define GROWUP_COROUTINE (1024*100)

#undef M_CO_IDLE_TIME
#define M_CO_IDLE_TIME (2*60)

#undef M_CORE_POOL_FACTOR
#define M_CORE_POOL_FACTOR (0.3)

#undef M_MAX_PROCS_FACTOR
#define M_MAX_PROCS_FACTOR (1.5)

#undef M_MAX_LOCAL_TASK_QUEUE
#define M_MAX_LOCAL_TASK_QUEUE (256)

#undef M_MAX_CO_WAIT_TIME
#define M_MAX_CO_WAIT_TIME (1800)