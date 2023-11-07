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

#ifndef COROUTINE_READY
#define COROUTINE_READY   (1)
#endif

#ifndef COROUTINE_RUNNING
#define COROUTINE_RUNNING (2)
#endif

#ifndef COROUTINE_SUSPEND
#define COROUTINE_SUSPEND (3)
#endif

#ifndef COROUTINE_DEAD
#define COROUTINE_DEAD	  (4)
#endif

// invalid coroutine id
#ifndef M_INVALID_COROUTINE_ID
#define M_INVALID_COROUTINE_ID (-1)
#endif

// main coroutine id
#ifndef M_MAIN_COROUTINE_ID
#define M_MAIN_COROUTINE_ID (0)
#endif

// for linux public stack
#ifndef M_PUBLIC_STACK_SIZE
#define M_PUBLIC_STACK_SIZE  1*1024*1024
#endif

// for windows stack
#ifndef M_PRIVATE_STACK_SIZE
#define M_PRIVATE_STACK_SIZE 1024*1024
#endif

// for linux private init stack
#ifndef M_LINUX_STACK_SIZE
#define M_LINUX_STACK_SIZE 1024*4
#endif

#ifndef GROWUP_COROUTINE
#define GROWUP_COROUTINE (1024*100)
#endif

