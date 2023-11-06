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
#include <functional>
#include <chrono>

class time_pool {
public:
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
    uint64_t _alloc_timer_id = 0;
    time_point _beg_time;

    // one hour
    time_pool(uint32_t max_interval = 3600);

    ~time_pool();

    bool update();

    // @interval: max interval is one hour
    uint64_t add_timer(uint32_t interval, std::function<void()>func);

    bool cancel_timer(uint64_t timer_id);

protected:
    void on_init();

    void** alloc_small_bucket();

    bool calc_bucket(time_point tp, uint32_t interval, uint32_t& big_bucket, uint32_t& small_bucket);
};