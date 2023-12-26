/*----------------------------------------------------------------
// Copyright 2021
// All rights reserved.
//
// author: 404558965@qq.com (xiaoquanjie)
// github: https://github.com/xiaoquanjie/cgo
// Created by xiaoqj on 2023/11/05
//----------------------------------------------------------------*/

#pragma once

#include "slist.h"
#include "concurrentqueue.h"
#include <functional>
#include <chrono>
#include <mutex>
#include <atomic>

// thread-unsafety
class time_pool {
protected:
    struct timer_node {
        uint64_t _timer_id = 0;
        uint64_t _expire = 0;
        std::function<void()> _cb;
    };

    using time_point = std::chrono::time_point<std::chrono::steady_clock>;
    using node_list = slist<timer_node>;

    uint32_t _big_bucket = 3600; // seconds
    uint32_t _small_bucket = 100; // 10ms

    void** _bucket = 0;
    uint32_t _big_iter = 0;
    uint32_t _small_iter = 0;
    uint64_t _alloc_timer_id = 1;
    time_point _beg_time;

    uint32_t _timer_count = 0;
public:
    // one hour
    time_pool(uint32_t max_interval = 3600);

    virtual ~time_pool();

    virtual bool update();

    uint32_t timer_count() const;

    // @interval: max interval is one hour
    uint64_t add_timer(uint32_t interval, std::function<void()> func);

    bool cancel_timer(uint64_t timer_id);

protected:
    timer_node alloc_timer_node(uint32_t interval, std::function<void()> func);

    void alloc_big_bucket();

    uint64_t timer_add(const timer_node& node);

    bool timer_cancel(uint64_t timer_id);

    void** alloc_small_bucket();

    bool calc_bucket(time_point tp, uint32_t interval, uint32_t& big_bucket, uint32_t& small_bucket);

    uint64_t alloc_timer_id(uint32_t big_bucket, uint32_t small_bucket);

    void decode_timer_id(uint64_t timer_id, uint32_t& big_bucket, uint32_t& small_bucket);

    uint32_t calc_iter(uint32_t big_bucket, int32_t small_bucket);

    uint32_t calc_iter(const timer_node& node);
};

// thread-safety
class async_time_pool : public time_pool {
protected:
    std::atomic_flag _flag;
    moodycamel::ConcurrentQueue<timer_node> _wait_list;

public:
    bool update() override;

    uint64_t async_add_timer(uint32_t interval, std::function<void()> func);

    void async_cancel_timer(uint64_t timer_id);

    bool is_prev_time_node(const timer_node& node);
};