/*----------------------------------------------------------------
// Copyright 2021
// All rights reserved.
//
// author: 404558965@qq.com (xiaoquanjie)
// github: https://github.com/xiaoquanjie/cgo
// Created by xiaoqj on 2023/10/31.
//----------------------------------------------------------------*/

#include "common/macro.h"

#ifdef _MSC_VER

#include "structure.h"
#include "common/print.h"
#include <cassert>
#include <string>

namespace cgo::coroutine {
    void __stdcall co_routine(LPVOID p) {
        // op routine
        auto co = (_co_st_ *) p;
        co->_routine();
        co->_status = COROUTINE_DEAD;
        ::SwitchToFiber(co->_mctx);
    }

    void win_init() {
        if (gmainctx) {
            return;
        }

        LPVOID ctx = ConvertThreadToFiberEx(nullptr, FIBER_FLAG_FLOAT_SWITCH);
        if (!ctx) {
            throw std::runtime_error(std::string("ConvertThreadToFiberEx error:") + std::to_string(GetLastError()));
        } else {
            gmainctx = ctx;
        }
    }

    uint64_t create(const std::function<void()> &routine, int stack) {
        win_init();

        auto co = _co_st_::alloc(routine, stack);
        assert(co != nullptr);

        LPVOID ctx = ::CreateFiberEx(stack, 0, FIBER_FLAG_FLOAT_SWITCH, co_routine, co);
        assert(ctx);
        if (!ctx) {
            _co_st_::free(co);
            throw std::bad_alloc();
        }

        co->_ctx = ctx;
        return co->get_coid();
    }

    // resume a coroutine, return the status of coroutine
    int resume(uint64_t co_id) {
        if (gcurno != M_INVALID_COROUTINE_ID) {
            return COROUTINE_NONE;
        }
        auto co = _co_st_::get_co(co_id);
        if (!co) {
            return COROUTINE_NONE;
        }

        win_init();

        switch (co->_status) {
            case COROUTINE_READY:
            case COROUTINE_SUSPEND: {
                co->_status = COROUTINE_RUNNING;
                gcurno = co_id;
                co->_mctx = gmainctx;
                ::SwitchToFiber(co->_ctx);
                auto status = co->_status;
                if (co->_status == COROUTINE_DEAD) {
                    _co_st_::free(co);
                }
                gcurno = M_INVALID_COROUTINE_ID;
                return status;
            }
            default:
                assert(co->_status != COROUTINE_SUSPEND && co->_status != COROUTINE_READY);
                break;
        }

        return COROUTINE_NONE;
    }

    void yield(uint64_t co_id) {
        auto co = _co_st_::get_co(co_id);
        co->_status = COROUTINE_SUSPEND;
        ::SwitchToFiber(co->_mctx);
    }
}

#endif