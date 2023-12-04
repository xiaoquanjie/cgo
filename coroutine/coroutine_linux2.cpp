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
            if (gmainco._curno != M_INVALID_COROUTINE_ID) {
                return;
            }
            auto co = _co_st_::get_co(co_id);
            if (!co) {
                return;
            }

            switch (co->_status) {
                case COROUTINE_SUSPEND:
                case COROUTINE_READY: {
                    co->_data = data;
                    co->_mctx = &gmainco._ctx;
                    co->_status = COROUTINE_RUNNING;
                    gmainco._curno = co_id;
                    swapcontext(co->_mctx, &co->_ctx);
                    co->_data = 0;
                    if (co->_status == COROUTINE_DEAD) {
                        _co_st_::free(co);
                    }
                    gmainco._curno = M_INVALID_COROUTINE_ID;
                    break;
                }
                default:
                    assert(false);
                    break;
            }

        }

        void yield(void** data) {
            if (gmainco._curno == M_INVALID_COROUTINE_ID) {
                return;
            }

            auto co = _co_st_::get_co(gmainco._curno);
            auto mctx = co->_mctx;
            co->_status = COROUTINE_SUSPEND;
            co->_mctx = 0;
            co->memory_check(co);
            swapcontext(&co->_ctx, mctx);

            if (data) {
                *data = co->_data;
                co->_data = 0;
            }
        }

        // yield
        void yield() {
            yield(0);
        }
    }
}
#endif