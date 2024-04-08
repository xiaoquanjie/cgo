/*----------------------------------------------------------------
// Copyright 2021
// All rights reserved.
//
// author: 404558965@qq.com (xiaoquanjie)
// github: https://github.com/xiaoquanjie/cgo
// Created by xiaoqj on 2023/11/05
//----------------------------------------------------------------*/

#include "time_pool.h"
#include "print.h"
#include <string>
#include <cstring>
#include <cassert>

template<typename Payload, int MaxInterval>
bool init_tpool(TimePoolInfo<Payload, MaxInterval> * info, uint32_t max_interval) {
    info->_bigbucket = MaxInterval;
    info->_begtime = std::chrono::steady_clock::now();

    for (int idx = 0; idx < MaxInterval; idx++) {
        info->_bucket[idx] = nullptr;
    }
    return true;
}

template<typename Payload, int MaxInterval>
void release_tpool(TimePoolInfo<Payload, MaxInterval> * info) {
    typedef typename TimePoolInfo<Payload, MaxInterval>::node_list node_list;
    for (uint32_t idx = 0; idx < info->_bigbucket; idx++) {
        auto big = info->_bucket[idx];
        if (!big) continue;

        for (uint32_t idx2 = 0; idx2 < info->_smallbucket; idx2++) {
            node_list* nl = big[idx2];
            if (nl) {
                assert(nl->size() == 0);
                delete nl;
            }
        }
        delete []big;
    }
}

template<typename Payload, int MaxInterval>
bool calc_bucket(TimePoolInfo<Payload, MaxInterval> * info,
                 const typename TimePoolInfo<Payload, MaxInterval>::time_point& tp,
                 uint32_t interval,
                 uint32_t& bigbucket,
                 uint32_t& smallbucket) {
    if (tp < info->_begtime) {
        assert(false);
        return false;
    }

    auto future_ms = (std::chrono::duration_cast<std::chrono::milliseconds>(tp - info->_begtime)).count();
    future_ms += interval;
    auto future_sec = future_ms / 1000;

    bigbucket = future_sec % info->_bigbucket;
    smallbucket = (future_ms % 1000) / (1000 / info->_smallbucket);
    return true;
}

template<typename Payload, int MaxInterval>
typename TimePoolInfo<Payload, MaxInterval>::node_list** alloc_small_bucket(uint32_t smallbucket) {
    auto small = new typename TimePoolInfo<Payload, MaxInterval>::node_list*[smallbucket];
    assert(small);
    if (small) {
        for (uint32_t idx = 0; idx < smallbucket; idx++) {
            small[idx] = nullptr;
        }
    }
    return small;
}

inline uint32_t calc_iterator(uint32_t smallbucket, uint32_t cur_big, uint32_t cur_small) {
    uint32_t iter = cur_big * smallbucket + cur_small;
    return iter;
}

template<typename Payload, int MaxInterval>
bool update_tpool(TimePoolInfo<Payload, MaxInterval> * info, typename TimePoolInfo<Payload, MaxInterval>::time_point& now) {
    uint64_t expire = (std::chrono::duration_cast<std::chrono::milliseconds>(now - info->_begtime)).count();

    uint32_t new_bigiter = 0;
    uint32_t new_smalliter = 0;
    if (!calc_bucket(info, now, 0, new_bigiter, new_smalliter)) {
        return false;
    }

    typedef typename TimePoolInfo<Payload, MaxInterval>::node_list node_list;
    uint32_t new_iterator = calc_iterator(info->_smallbucket, new_bigiter, new_smalliter);
    uint32_t old_iterator = calc_iterator(info->_smallbucket, info->_bigiter, info->_smalliter);

    // reset
    info->_bigiter = new_bigiter;
    info->_smalliter = new_smalliter;

    for (; old_iterator <= new_iterator; old_iterator++) {
        uint32_t big = old_iterator / info->_smallbucket;
        uint32_t small = old_iterator % info->_smallbucket;

        auto bucket = info->_bucket[big];
        if (!bucket) {
            continue;
        }

        node_list* nl = bucket[small];
        if (!nl) {
            continue;
        }

        for (auto iter = nl->begin(); iter != nl->end();) {
            auto& node = (*iter);
            if (node->expire <= expire) {
                info->_count--;
                info->_notify_node(node);
                iter = nl->erase(iter);
                delete node;
            } else {
                iter++;
            }
        }

        if (nl->empty()) {
            free((void*)bucket[small]);
            bucket[small] = nullptr;
        }
    }

    return true;
}

template<typename Payload, int MaxInterval>
inline uint64_t alloc_timeid(TimePoolInfo<Payload, MaxInterval> * info, uint32_t bigbucket, uint32_t smallbucket) {
    uint64_t timer_id = (bigbucket * info->_smallbucket + smallbucket);
    timer_id = timer_id << 32;
    timer_id += info->_allocid++;
    if (info->_allocid == 0xFFFFFFFE) {
        info->_allocid = 1;
    }
    return timer_id;
}

template<typename Payload, int MaxInterval>
inline void decode_timeid(TimePoolInfo<Payload, MaxInterval> * info, uint64_t tid, uint32_t& bigbucket, uint32_t& smallbucket) {
    uint32_t high32Bit = (tid >> 32);
    bigbucket = high32Bit / info->_smallbucket;
    smallbucket = high32Bit % info->_smallbucket;
}

template<typename Payload, int MaxInterval>
typename TimePoolInfo<Payload, MaxInterval>::tnode* alloc_node(TimePoolInfo<Payload, MaxInterval> * info, uint32_t interval) {
    if (interval <= 0 || interval > info->_bigbucket * 1000) {
        return nullptr;
    }

    auto now = std::chrono::steady_clock::now();
    uint32_t bigbucket = 0;
    uint32_t smallbucket = 0;
    if (!calc_bucket(info, now, interval, bigbucket, smallbucket)) {
        return nullptr;
    }

    auto node = new typename TimePoolInfo<Payload, MaxInterval>::tnode;
    node->id = alloc_timeid(info, bigbucket, smallbucket);
    node->expire = (std::chrono::duration_cast<std::chrono::milliseconds>(now - info->_begtime)).count() + interval;
    return node;
}

template<typename Payload, int MaxInterval>
void add_tpool_node(TimePoolInfo<Payload, MaxInterval> * info, typename TimePoolInfo<Payload, MaxInterval>::tnode* node) {
    uint32_t bigbucket = 0;
    uint32_t smallbucket = 0;
    decode_timeid(info, node->id, bigbucket, smallbucket);

    typedef typename TimePoolInfo<Payload, MaxInterval>::node_list node_list;
    auto bucket = info->_bucket[bigbucket];
    if (!bucket) {
        bucket = info->_bucket[bigbucket] = alloc_small_bucket<Payload, MaxInterval>(info->_smallbucket);
    }

    auto nl = bucket[smallbucket];
    if (!nl) {
        nl = new node_list;
        bucket[smallbucket] = nl;
    }

    info->_count++;
    nl->push_back(node);
}

void time_pool::notify_node(Node* node) {
    node->payload();
}

time_pool::time_pool(uint32_t max_interval) {
    init_tpool(&this->_info, max_interval);
    this->_info._notify_node = &time_pool::notify_node;
}

time_pool::~time_pool() {
    release_tpool(&this->_info);
}

bool time_pool::update() {
    auto now = std::chrono::steady_clock::now();
    return update_tpool(&this->_info, now);
}

uint32_t time_pool::timer_count() const {
    return this->_info._count;
}

// @interval: ms
uint64_t time_pool::add_timer(uint32_t interval, const Payload& func) {
    Node* node = alloc_node(&this->_info, interval);
    if (!node) {
        assert(false);
        return 0;
    }

    node->payload = func;
    add_tpool_node(&this->_info, node);
    return node->id;
}

///////////////////////////////////////////////////////////

void async_time_pool::notify_node(Node* node) {
    node->payload();
}

async_time_pool::async_time_pool(uint32_t max_interval) {
    init_tpool(&this->_info, max_interval);
    this->_info._notify_node = &async_time_pool::notify_node;
}

async_time_pool::~async_time_pool() {
    release_tpool(&this->_info);
}

bool async_time_pool::update() {
    auto now = std::chrono::steady_clock::now();
    uint64_t expire = (std::chrono::duration_cast<std::chrono::milliseconds>(now - _info._begtime)).count();
    Node * node;
    while (this->_waits.try_dequeue(node)) {
        if (node->expire <= expire) {
            node->payload();
            delete node;
        } else {
            add_tpool_node(&this->_info, node);
        }
    }

    return update_tpool(&this->_info, now);
}

uint32_t async_time_pool::timer_count() const {
    return this->_info._count + this->_waits.size_approx();
}

uint64_t async_time_pool::add_timer(uint32_t interval, const Payload& func) {
    auto node = alloc_node(&this->_info, interval);
    if (!node) {
        assert(false);
        return 0;
    }

    node->payload = func;
    this->_waits.enqueue(node);
    return node->id;
}

integer_async_time_pool::integer_async_time_pool(void(*notify)(Node*), uint32_t max_interval) {
    init_tpool(&_info, max_interval);
    _info._notify_node = notify;
}

integer_async_time_pool::~integer_async_time_pool() {
    release_tpool(&_info);
}

bool integer_async_time_pool::update() {
    auto now = std::chrono::steady_clock::now();
    uint64_t expire = (std::chrono::duration_cast<std::chrono::milliseconds>(now - _info._begtime)).count();
    Node* node;
    for (int i = 0; i < 10000; i++) {
        if (!_waits.try_dequeue(node)) {
            break;
        }
        if (node->expire <= expire) {
            _info._notify_node(node);
            delete node;
        } else {
            add_tpool_node(&this->_info, node);
        }
    }

    return update_tpool(&this->_info, now);
}

uint32_t integer_async_time_pool::timer_count() const {
    return _info._count;
}

uint64_t integer_async_time_pool::add_timer(uint32_t interval, const Payload& payload) {
    auto node = alloc_node(&_info, interval);
    if (!node) {
        assert(false);
        return 0;
    }

    node->payload = payload;
    _waits.enqueue(node);
    return node->id;
}