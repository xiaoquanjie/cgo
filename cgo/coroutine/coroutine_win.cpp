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
#include <assert.h>

namespace cgo {
    namespace coroutine {
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

            LPVOID ctx = ConvertThreadToFiberEx(0, FIBER_FLAG_FLOAT_SWITCH);
            if (!ctx) {
                DWORD error = GetLastError();
                throw error;
            } else {
                gmainctx = ctx;
            }
        }

        uint64_t create(std::function<void()> routine, int stack, const char* file, int line) {
            win_init();

            auto co = new _co_st_;
            assert(co != 0);
            if (stack <= 0) {
                stack = M_PRIVATE_STACK_SIZE;
            }

            LPVOID ctx = ::CreateFiberEx(stack, 0, FIBER_FLAG_FLOAT_SWITCH, co_routine, co);
            assert(ctx);
            if (!ctx) {
                _co_st_::free(co);
                throw std::string("memory not enough");
            }
           
            _co_st_::init(co, routine, stack, file, line);
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
                    assert(false);
                    break;
            }

            return COROUTINE_NONE;
        }

		void yield() {
			if (gcurno == M_INVALID_COROUTINE_ID) {
				return;
			}

            auto co_id = gcurno;
            auto co = _co_st_::get_co(co_id);
			co->_status = COROUTINE_SUSPEND;

			::SwitchToFiber(co->_mctx);
		}
    }
}
#endif