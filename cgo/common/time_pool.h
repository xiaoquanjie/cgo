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
#include "macro.h"
#include <functional>
#include <chrono>
#include <list>

template<typename Payload, int MaxInterval>
struct TimePoolInfo {
    struct tnode {
        uint64_t id = 0;
        uint64_t expire = 0;
        Payload payload;
    };

    using time_point = std::chrono::time_point<std::chrono::steady_clock>;
    using node_list = std::list<tnode*>;

    node_list** _bucket[MaxInterval];
    time_point _begtime;

    // seconds
    uint32_t _bigbucket = 1800;
    // 10ms
    uint32_t _smallbucket = 100;

    // iterator record
    uint32_t _bigiter = 0;
    uint32_t _smalliter = 0;

    uint64_t _allocid = 1;
    uint32_t _count = 0;

    void(*_notify_node)(tnode*) = 0;
};

// thread-unsafety
class time_pool {
    static const int MaxInterval = M_MAX_CO_WAIT_TIME;
    typedef std::function<void()> Payload;
    TimePoolInfo<Payload, MaxInterval> _info;
    typedef typename TimePoolInfo<Payload, MaxInterval>::tnode Node;

    static void notify_node(Node*);
public:
    // one hour
    explicit time_pool(uint32_t max_interval = 1800);

    ~time_pool();

    bool update();

    [[nodiscard]]
    uint32_t timer_count() const;

    // @interval: max interval is one hour
    uint64_t add_timer(uint32_t interval, const Payload& func);
};

// thread-safety
class async_time_pool {
    static const int MaxInterval = M_MAX_CO_WAIT_TIME;
    typedef std::function<void()> Payload;
    TimePoolInfo<Payload, MaxInterval> _info;
    typedef typename TimePoolInfo<Payload, MaxInterval>::tnode Node;

    static void notify_node(Node*);
protected:
    moodycamel::ConcurrentQueue<typename TimePoolInfo<Payload, MaxInterval>::tnode*> _waits;

public:
    explicit async_time_pool(uint32_t max_interval = 1800);

    ~async_time_pool();

    bool update();

    uint32_t timer_count() const;

    uint64_t add_timer(uint32_t interval, const Payload& func);
};

class integer_async_time_pool {
public:
    static const int MaxInterval = M_MAX_CO_WAIT_TIME;
    typedef uint64_t Payload;
    typedef typename TimePoolInfo<Payload, MaxInterval>::tnode Node;
protected:
    TimePoolInfo<Payload, MaxInterval> _info;
    moodycamel::ConcurrentQueue<Node*> _waits;

public:
    integer_async_time_pool(void(*notify)(Node*), uint32_t max_interval);

    ~integer_async_time_pool();

    bool update();

    uint32_t timer_count() const;

    uint64_t add_timer(uint32_t interval, const Payload& payload);
};