/*----------------------------------------------------------------
// Copyright 2021
// All rights reserved.
//
// author: 404558965@qq.com (xiaoquanjie)
// github: https://github.com/xiaoquanjie/cgo
// Created by xiaoqj on 2023/11/03
//----------------------------------------------------------------*/

#include "common/print.h"
#include "structure.h"
#include <cassert>

#ifdef __GNUC__
#include <cstring>
#endif

namespace cgo::coroutine {
    uint64_t curid() {
        uint64_t id = gcurno;
        return id;
    }

    uint64_t _co_st_::get_coid() {
        return get_coid(this);
    }

    _co_st_* _co_st_::alloc(const std::function<void()>& routine, int stack) {
        auto co = new _co_st_;
        assert(co != nullptr && stack >= 1024*2);
        return init(co, routine, stack);
    }

    _co_st_* _co_st_::init(_co_st_* co, const std::function<void()>& routine, int stack) {
        co->_ssize = stack;
        co->_status = COROUTINE_READY;
        co->_routine = routine;

#ifdef __GNUC__
        // init stack
        co->_stack = (char*) malloc(co->_ssize);
        uint64_t* d = (uint64_t*)co->_stack;
        *d = 0xFEFEFEFEFEFEFEFE;
#endif
        return co;
    }

    void _co_st_::free(_co_st_ *co) {
        if (co) {
#ifdef __GNUC__
        // call global free
        ::free(co->_stack);
#else
        DeleteFiber(co->_ctx);
#endif
        } else {
            assert(false);
        }
        delete co;
    }

    void _co_st_::free(uint64_t co_id) {
        auto co = get_co(co_id);
        _co_st_::free(co);
    }

    _co_st_* _co_st_::get_co(uint64_t co_id) {
        auto co = (_co_st_*)uintptr_t(co_id);
        return co;
    }

    uint64_t _co_st_::get_coid(_co_st_* co) {
        auto co_id = (uintptr_t)co;
        return (uint64_t)co_id;
    }

    bool _co_st_::memory_check(_co_st_* co) {
#ifdef __GNUC__
        assert(co->_stack);
        uint64_t* d = (uint64_t*)co->_stack;
        assert(*d == 0xFEFEFEFEFEFEFEFE);
        return true;
#else
        return true;
#endif
    }

    thread_local volatile uint64_t gcurno = M_INVALID_COROUTINE_ID;
#ifdef _MSC_VER
    thread_local LPVOID volatile gmainctx = nullptr;
#else
    static thread_local ucontext_t glocal_mainctx;
    thread_local ucontext_t* volatile gmainctx = &glocal_mainctx;
#endif
}