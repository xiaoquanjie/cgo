/*----------------------------------------------------------------
// Copyright 2021
// All rights reserved.
//
// author: 404558965@qq.com (xiaoquanjie)
// github: https://github.com/xiaoquanjie/cgo
// Created by xiaoqj on 2023/10/31.
//----------------------------------------------------------------*/

#include "structure.h"
#include "coroutine.h"

namespace cgo::coroutine {
    bool set_udata(uint64_t co_id, void* data) {
        auto co = _co_st_::get_co(co_id);
        if (co) {
            co->_data = data;
            return true;
        }
        return false;
    }

    void* get_udata(uint64_t co_id) {
        auto co = _co_st_::get_co(co_id);
        if (co) {
            return co->_data;
        }
        return nullptr;
    }

    void yield() {
        if (gcurno == M_INVALID_COROUTINE_ID) {
            return;
        }

        auto co_id = gcurno;
        yield(co_id);
    }

    void run(const std::function<void()>& routine, int stack) {
        auto co_id = create(routine, stack);
        resume(co_id);
    }

    void run(const std::function<void()>& routine) {
        run(routine, 0);
    }

    int memory() {
        return gmem._mem;
    }

}

