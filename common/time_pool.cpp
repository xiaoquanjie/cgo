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
#include <string.h>
#include <assert.h>

time_pool::time_pool(uint32_t max_interval) {
    if (max_interval != 0)  {
        _big_bucket = max_interval;
    }
    this->_beg_time = std::chrono::steady_clock::now();
}

time_pool::~time_pool() {
    if (!this->_bucket) {
        return;
    }

    for (uint32_t i = 0; i < this->_big_bucket; ++i) {
        void** small = (void**)this->_bucket[i];
        if (!small) {
            continue;
        }
        for (uint32_t j = 0; j < this->_small_bucket; ++j) {
            void* node = small[j];
            if (node) {
                auto nl = (node_list*)node;
                delete nl;
            }
        }
        free(small);
    }

    free(this->_bucket);
}

bool time_pool::update() {
    if (!this->_bucket) {
        return false;
    }

    auto now = std::chrono::steady_clock::now();
    auto expire = (std::chrono::duration_cast<std::chrono::milliseconds>(now - this->_beg_time)).count();
    uint32_t big_bucket_loc = 0;
    uint32_t small_bucket_loc = 0;

    if (!calc_bucket(now, 0, big_bucket_loc, small_bucket_loc)) {
        return false;
    }

    uint32_t start_iter = calc_iter(this->_big_iter, this->_small_iter);
    uint32_t end_iter = calc_iter(big_bucket_loc, small_bucket_loc);

    bool busy = false;
    auto cmp = [expire, &busy](timer_node& node) -> bool {
        if (node._expire <= (uint64_t)expire) {
            node._cb();
            busy = true;
            return false;
        }
        return true;
    };

    for (; start_iter <= end_iter; start_iter++) {
        int b = start_iter / this->_small_bucket;
        int s = start_iter % this->_small_bucket;

        void** bucket = (void**)this->_bucket[b];
        if (!bucket) {
            continue;
        }

        auto nl = (node_list*)bucket[s];
        if (!nl) {
            continue;
        }

        nl->iterate(cmp);
    }

    this->_big_iter = big_bucket_loc;
    this->_small_iter = small_bucket_loc;
    return busy;
}

// @interval: ms
uint64_t time_pool::add_timer(uint32_t interval, std::function<void()> func) {
    auto node = alloc_timer_node(interval, func);
    if (node._timer_id == 0) {
        return 0;
    }

    return timer_add(node);
}

uint64_t time_pool::timer_add(const timer_node& node) {
    alloc_big_bucket();

    uint32_t big_bucket_loc = 0;
    uint32_t small_bucket_loc = 0;
    decode_timer_id(node._timer_id, big_bucket_loc, small_bucket_loc);

    void** bucket = (void**)this->_bucket[big_bucket_loc];
    if (!bucket) {
        bucket = alloc_small_bucket();
        this->_bucket[big_bucket_loc] = bucket;
    }

    node_list* nl = (node_list*)(bucket[small_bucket_loc]);
    if (!nl) {
        nl = new node_list();
        bucket[small_bucket_loc] = nl;
    }

    nl->push(node);
    return node._timer_id;
}

bool time_pool::cancel_timer(uint64_t timer_id) {
    return timer_cancel(timer_id);
}

bool time_pool::timer_cancel(uint64_t timer_id) {
    if (!this->_bucket) {
        return false;
    }

    uint32_t big = 0;
    uint32_t small = 0;
    decode_timer_id(timer_id, big, small);

    if (big >= this->_big_bucket) {
        return false;
    }
    if (small >= this->_small_bucket) {
        return false;
    }

    void** bucket = (void**)this->_bucket[big];
    if (!bucket) {
        return false;
    }

    node_list* nl = (node_list*)(bucket[small]);
    if (!nl) {
        return false;
    }

    bool ret = false;
    nl->iterate([&timer_id, &ret](timer_node& node)->bool {
        if (node._timer_id == timer_id) {
            ret = true;
            return false;
        } else {
            return true;
        }
    }, true);

    return ret;
}

time_pool::timer_node time_pool::alloc_timer_node(uint32_t interval, std::function<void()> func) {
    timer_node node;
    node._timer_id = 0;

    if (interval <= 0 || interval > this->_big_bucket * 1000) {
        return node;
    }

    auto now = std::chrono::steady_clock::now();
    uint32_t big_bucket_loc = 0;
    uint32_t small_bucket_loc = 0;
    if (!calc_bucket(now, interval, big_bucket_loc, small_bucket_loc)) {
        return node;
    }

    node._timer_id = alloc_timer_id(big_bucket_loc, small_bucket_loc);
    node._expire = (std::chrono::duration_cast<std::chrono::milliseconds>(now - this->_beg_time)).count() + interval;
    node._cb = func;
    return node;
}

void time_pool::alloc_big_bucket() {
    if (this->_bucket) {
        return;
    }

    this->_bucket = (void**) malloc(sizeof(void*)*this->_big_bucket);
    if (!this->_bucket) {
        assert(this->_bucket);
        return;
    }

    for (uint32_t idx = 0; idx < this->_big_bucket; ++idx) {
        this->_bucket[idx] = 0;
    }
}

void** time_pool::alloc_small_bucket() {
    void** small = (void**) malloc(sizeof(void*)*this->_small_bucket);
    assert(small);
    memset(small, 0, sizeof(void*)*this->_small_bucket);
    return small;
}

bool time_pool::calc_bucket(time_point tp, uint32_t interval, uint32_t& big_bucket, uint32_t& small_bucket) {
    if (tp < this->_beg_time) {
        assert(false);
        return false;
    }

    auto future_ms = (std::chrono::duration_cast<std::chrono::milliseconds>(tp - this->_beg_time)).count();
    future_ms += interval;
    auto future_sec = future_ms / 1000;

    big_bucket = future_sec % this->_big_bucket;
    small_bucket = (future_ms % 1000) / (1000 / this->_small_bucket);
    return true;
}

uint64_t time_pool::alloc_timer_id(uint32_t big_bucket, uint32_t small_bucket) {
    uint64_t timer_id = (big_bucket * this->_small_bucket + small_bucket);
    timer_id = timer_id << 32;
    timer_id += this->_alloc_timer_id++;
    if (this->_alloc_timer_id == 0xFFFFFFFE) {
        this->_alloc_timer_id = 1;
    }
    return timer_id;
}

void time_pool::decode_timer_id(uint64_t timer_id, uint32_t& big_bucket, uint32_t& small_bucket) {
    uint32_t high32Bit = (timer_id >> 32);
    big_bucket = high32Bit / this->_small_bucket;
    small_bucket = high32Bit % this->_small_bucket;
}

uint32_t time_pool::calc_iter(uint32_t big_bucket, int32_t small_bucket) {
    uint32_t iter = big_bucket * this->_small_bucket + small_bucket;
    return iter;
}

uint32_t time_pool::calc_iter(const timer_node& node) {
    uint32_t big_bucket_loc = 0;
    uint32_t small_bucket_loc = 0;
    decode_timer_id(node._timer_id, big_bucket_loc, small_bucket_loc);
    return calc_iter(big_bucket_loc, small_bucket_loc);
}

///////////////////////////////////////////////////////////

bool async_time_pool::update() {
    if (this->_flag.test_and_set()) {
        return false;
    }

    timer_node node;
    while (_wait_list.try_dequeue(node)) {
        if (node._cb) {
            if (is_prev_time_node(node)) {
                node._cb();
            } else {
                this->timer_add(node);
            }
        } else {
            this->timer_cancel(node._timer_id);
        }
    }

    auto ret = time_pool::update();
    //M_CO_DEBUG_PRINT("release lock\n");
    this->_flag.clear();
    return ret;
}

uint64_t async_time_pool::async_add_timer(uint32_t interval, std::function<void()> func) {
    auto node = alloc_timer_node(interval, func);
    if (node._timer_id == 0) {
        return 0;
    }

    _wait_list.enqueue(node);
    return node._timer_id;
}

void async_time_pool::async_cancel_timer(uint64_t timer_id) {
    _wait_list.enqueue(timer_node{timer_id, 0, nullptr});
}

bool async_time_pool::is_prev_time_node(const timer_node& node) {
    if (calc_iter(node) < calc_iter(this->_big_iter, this->_small_iter)) {
        return true;
    }
    return false;
}