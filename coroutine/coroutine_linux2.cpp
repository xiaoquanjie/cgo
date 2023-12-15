/*----------------------------------------------------------------
// Copyright 2021
// All rights reserved.
//
// author: 404558965@qq.com (xiaoquanjie)
// github: https://github.com/xiaoquanjie/cgo
// Created by xiaoqj on 2023/11/07.
//----------------------------------------------------------------*/

#include "macro.h"

#ifndef M_PLATFORM_WIN

#include "structure.h"
#include <assert.h>
#include <string.h>

namespace cgo {
    namespace coroutine {
        void co_routine(_co_st_ *c) {
            // op routine
            c->_routine();
            c->_status = COROUTINE_DEAD;
            swapcontext(&c->_ctx, c->_mctx);
        }

        // create one new coroutine
        uint64_t create(std::function<void()> routine, int stack, const char* file, int line) {
            auto co = _co_st_::alloc(routine, stack, file, line);
            getcontext(&co->_ctx);
            //co->_ctx.uc_link = 0;
            co->_ctx.uc_stack.ss_size = co->_ssize;
            co->_ctx.uc_stack.ss_sp = co->_stack;
            makecontext(&co->_ctx, (void(*)(void))co_routine, 1, co);
            return co->get_coid();
        }

        // resume a coroutine
        void resume(uint64_t co_id, void* data) {
            if (gcurno != M_INVALID_COROUTINE_ID) {
                return;
            }
            auto co = _co_st_::get_co(co_id);
            if (!co) {
                return;
            }

            switch (co->_status) {
                case COROUTINE_SUSPEND:
                    co->_data = data;
                case COROUTINE_READY: {
                    co->_mctx = gmainctx;
                    co->_status = COROUTINE_RUNNING;
                    gcurno = co_id;
                    swapcontext(co->_mctx, &co->_ctx);
                    co->_data = 0;
                    if (co->_status == COROUTINE_DEAD) {
                        _co_st_::free(co);
                    }
                    gcurno = M_INVALID_COROUTINE_ID;
                    break;
                }
                default:
                    assert(co->_status != COROUTINE_SUSPEND && co->_status != COROUTINE_READY);
                    break;
            }

        }

        void yield(void*& data) {
            if (gcurno == M_INVALID_COROUTINE_ID) {
                return;
            }

            auto co_id = gcurno;
            auto co = _co_st_::get_co(co_id);
            co->_status = COROUTINE_SUSPEND;
            co->memory_check(co);

            swapcontext(&co->_ctx, co->_mctx);

            co = _co_st_::get_co(co_id);
            data = co->_data;
            co->_data = 0;
        }
    }
}
#endif