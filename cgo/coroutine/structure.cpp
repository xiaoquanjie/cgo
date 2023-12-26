/*----------------------------------------------------------------
// Copyright 2021
// All rights reserved.
//
// author: 404558965@qq.com (xiaoquanjie)
// github: https://github.com/xiaoquanjie/cgo
// Created by xiaoqj on 2023/11/03
//----------------------------------------------------------------*/

#include "../common/print.h"
#include "structure.h"
#include <assert.h>
#include <string.h>
#include <mutex>
#include <shared_mutex>

namespace cgo {
    namespace coroutine {
        uint64_t _co_st_::get_coid() {
            return get_coid(this);
        }

        _co_st_* _co_st_::alloc(std::function<void()> routine, int stack, const char* file, int line) {
            auto co = new _co_st_;
            assert(co != 0);
            if (stack <= 0) {
                stack = M_PRIVATE_STACK_SIZE;
            }
            return init(co, routine, stack, file, line);
        }

        _co_st_* _co_st_::init(_co_st_* co, std::function<void()> routine, int stack, const char* file, int line) {
            co->_ssize = stack;
            co->_status = COROUTINE_READY;
            co->_routine = routine;
            co->_file = file;
            co->_line = line;

#ifdef __GNUC__
            // init stack
            co->_stack = (char*) malloc(co->_ssize);
            memset(co->_stack, 0, 8);
#endif
            gmem.add(sizeof(_co_st_));
            gmem.add(co->_ssize);
            return co;
        }

        void _co_st_::free(_co_st_ *co) {
            assert(co != 0);
#ifdef __GNUC__
            if (co->_stack) {
                // call global free
                ::free(co->_stack);
            }
#else
            DeleteFiber(co->_ctx);
#endif
            gmem.dec(sizeof(_co_st_));
            gmem.dec(co->_ssize);
            delete co;
        }

        void _co_st_::free(uint64_t co_id) {
            auto co = get_co(co_id);
            _co_st_::free(co);
        }

        _co_st_* _co_st_::get_co(uint64_t co_id) {
            _co_st_* co = (_co_st_*)uintptr_t(co_id);
            return co;
        }

        uint64_t _co_st_::get_coid(_co_st_* co) {
            uintptr_t co_id = (uintptr_t)co;
            return (uint64_t)co_id;
        }

        bool _co_st_::memory_check(_co_st_* co) {
#ifdef __GNUC__
            assert(co->_stack);
            uint64_t* d = (uint64_t*)co->_stack;
            assert(*d == 0);
            return true;
#else
            return true;
#endif
        }

        void _memory_st_::add(size_t s) {
            this->_mem += (int)s;
        }

        void _memory_st_::dec(size_t s) {
            this->_mem -= (int)s;
        }

        thread_local volatile uint64_t gcurno = M_INVALID_COROUTINE_ID;
#ifdef _MSC_VER
        thread_local LPVOID volatile gmainctx = 0;
#else
        static thread_local ucontext_t glocal_mainctx;
        thread_local ucontext_t* volatile gmainctx = &glocal_mainctx;
#endif

        _memory_st_ gmem;
    }
}