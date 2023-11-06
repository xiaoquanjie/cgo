/*----------------------------------------------------------------
// Copyright 2021
// All rights reserved.
//
// author: 404558965@qq.com (xiaoquanjie)
// github: https://github.com/xiaoquanjie/cgo
// Created by xiaoqj on 2023/10/31.
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

        void save_stack(_co_st_ *c, char *top) {
            char dummy = 0;
            c->_ssize = top - &dummy;
            assert(c->_ssize <= M_LINUX_PUBLIC_STACK_SIZE && c->_ssize > 0);
            if (c->_scap < c->_ssize || (c->_scap - c->_ssize > 1024)) {
                gmem.dec(c->_scap);
                free(c->_stack);

                c->_scap = c->_ssize;
                c->_stack = (char *) malloc(c->_scap);
                gmem.add(c->_scap);
            }
            memcpy(c->_stack, &dummy, c->_ssize);
        }

        // create one new coroutine
        int64_t create(std::function<void()> routine, const char* file, int line) {
            auto co = gschedule_st.alloc_co(routine, file, line);
            getcontext(&co->_ctx);
            co->_ctx.uc_link = 0;
            co->_ctx.uc_stack.ss_size = co->_scap;
            co->_ctx.uc_stack.ss_sp = co->_stack;
            makecontext(&co->_ctx, (void(*)(void))co_routine, 1, co);
            return co->_no;
        }

        // resume a coroutine
        void resume(int64_t co_id) {
            if (gmainco._curno != -1) {
                return;
            }
            auto co = gschedule_st.get_co(co_id);
            if (!co) {
                return;
            }

            switch (co->_status) {
                case COROUTINE_SUSPEND:
                case COROUTINE_READY: {
                    co->_mctx = &gmainco._ctx;
                    // copy private stack data to public stack
                    memcpy(gmainco._stack + M_LINUX_PUBLIC_STACK_SIZE - co->_ssize, co->_stack, co->_ssize);
                    // modify context stack
                    uint64_t use_stack = 0;
                    uint64_t last_stack_size = 0;
                    if (co->_status == COROUTINE_READY) {
                        last_stack_size = co->_ssize;
                    } else {
                        last_stack_size = M_LINUX_PUBLIC_STACK_SIZE;
                    }
#ifdef __CYGWIN__
                    use_stack = (uint64_t)(co->_pstack + last_stack_size) - (uint64_t)(co->_ctx.uc_mcontext.rsp);
                    co->_ctx.uc_mcontext.rsp = (uint64_t)(gmainco._stack + M_LINUX_PUBLIC_STACK_SIZE - use_stack);
#else
                    use_stack = (uint64_t)(co->_pstack + last_stack_size) - (uint64_t)co->_ctx.uc_mcontext.gregs[REG_RSP];
                    co->_ctx.uc_mcontext.gregs[REG_RSP] =(uint64_t)(gmainco._stack + M_LINUX_PUBLIC_STACK_SIZE - use_stack);
#endif
                    co->_ctx.uc_stack.ss_size = M_LINUX_PUBLIC_STACK_SIZE;
                    co->_ctx.uc_stack.ss_sp = gmainco._stack;
                    co->_pstack = (char*)co->_ctx.uc_stack.ss_sp;
                    co->_status = COROUTINE_RUNNING;
                    gmainco._curno = co_id;
                    swapcontext(co->_mctx, &co->_ctx);
                    if (co->_status == COROUTINE_DEAD) {
                        gschedule_st.free_co(co);
                    }
                    gmainco._curno = -1;
                    break;
                }
                default:
                    assert(false);
                    break;
            }

        }

        // yield
        void yield() {
            if (gmainco._curno == -1) {
                return;
            }

            auto co = gschedule_st.get_co(gmainco._curno);
            auto mctx = co->_mctx;
            save_stack(co, gmainco._stack + M_LINUX_PUBLIC_STACK_SIZE);
            co->_status = COROUTINE_SUSPEND;
            co->_mctx = 0;
            swapcontext(&co->_ctx, mctx);
        }

        int num() {
            auto n = gschedule_st._no - gschedule_st._freenos.size();
            return n;
        }
    }
}
#endif