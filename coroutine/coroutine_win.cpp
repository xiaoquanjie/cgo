/*----------------------------------------------------------------
// Copyright 2021
// All rights reserved.
//
// author: 404558965@qq.com (xiaoquanjie)
// github: https://github.com/xiaoquanjie/cgo
// Created by xiaoqj on 2023/10/31.
//----------------------------------------------------------------*/

#include "macro.h"

#ifdef M_PLATFORM_WIN

#include "structure.h"
#include "../common/print.h"
#include <assert.h>

namespace cgo {
    namespace coroutine {
        void __stdcall co_routine(LPVOID p) {
            // op routine
            auto co = (_co_st_ *) p;
            co->_routine();
            co->_status = COROUTINE_DEAD;
            gmem.dec(M_WIN_STACK_SIZE);
            gcocount._count -= 1;
            ::SwitchToFiber(co->_mctx);
        }

        void win_init() {
            if (gmainco._ctx) {
                return;
            }

            LPVOID ctx = ConvertThreadToFiberEx(0, FIBER_FLAG_FLOAT_SWITCH);
            if (!ctx) {
                DWORD error = GetLastError();
                throw error;
            } else {
                gmainco._ctx = ctx;
            }
        }

        int64_t create(std::function<void()> routine) {
            win_init();

            auto co = gschedule_st.alloc_co(routine);
            LPVOID ctx = ::CreateFiberEx(M_WIN_STACK_SIZE, 0, FIBER_FLAG_FLOAT_SWITCH, co_routine, co);
            if (!ctx) {
                M_CO_DEBUG_PRINT("windows create coroutine error:%d\n", ::GetLastError());
                gschedule_st.free_co(co);
                return M_INVALID_COROUTINE_ID;
            }

            co->_ctx = ctx;
            co->_mctx = gmainco._ctx;
            gmem.add(M_WIN_STACK_SIZE);
            gcocount._count += 1;
            return co->_no;
        }

        void resume(int64_t co_id) {
            if (gmainco._curno != -1) {
                return;
            }
            auto co = gschedule_st.get_co(co_id);
            if (!co) {
                return;
            }

            switch (co->_status) {
                case COROUTINE_READY:
                case COROUTINE_SUSPEND: {
                    co->_status = COROUTINE_RUNNING;
                    gmainco._curno = co_id;
                    ::SwitchToFiber(co->_ctx);
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

        void yield() {
            if (gmainco._curno == -1) {
                return;
            }

            auto co = gschedule_st.get_co(gmainco._curno);
            co->_status = COROUTINE_SUSPEND;
            ::SwitchToFiber(co->_mctx);
        }

        int num() {
            return gcocount._count;
        }

        int num_in_thread() {
            auto n = gschedule_st._no - gschedule_st._freenos.size();
            return n;
        }
    }
}
#endif