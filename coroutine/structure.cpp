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
        _co_st_* _co_st_::alloc(int stack) {
            auto co = new _co_st_;
            co->_ssize = stack;
#ifndef M_PLATFORM_WIN
            // init stack
            co->_stack = (char*) malloc(co->_ssize);
            memset(co->_stack, 0, 8);
            gmem.add(co->_ssize);
            //co->_pstack = co->_stack;
#endif
            assert(co != 0);
            return co;
        }

        void _co_st_::free(_co_st_ *co) {
#ifndef M_PLATFORM_WIN
            if (co->_stack) {
                // call global free
                ::free(co->_stack);
                gmem.dec(co->_ssize);
            }
#endif
            assert(co != 0);
            delete co;
        }

        bool _co_st_::memory_check(_co_st_* co) {
#ifndef M_PLATFORM_WIN
            assert(co->_stack);
            uint64_t* d = (uint64_t*)co->_stack;
            assert(*d == 0);
            return true;
#else
            return true;
#endif
        }

        _schedule_st_::~_schedule_st_() {
            for (int i = 0; i < this->_no; ++i) {
                auto co = this->_co[i];
                if (co) {
					co->_status = COROUTINE_DEAD;
                }
            }
        }

        _co_st_* _schedule_st_::alloc_co(std::function<void()> routine, int stack, const char* file, int line) {
            auto no = this->alloc_no();
            auto co = _co_st_::alloc(stack <= 0 ? M_PRIVATE_STACK_SIZE : stack);
            co->_no = no;
            co->_status = COROUTINE_READY;
            co->_routine = routine;
            co->_file = file;
            co->_line = line;
            this->_co[no] = co;
            gmem.add(sizeof(_co_st_));
            return co;
        }

        void _schedule_st_::free_co(_co_st_* co) {
            auto no = co->_no;
            _co_st_::free(co);
            gmem.dec(sizeof(_co_st_));

            this->_freenos.enqueue(no);
            this->_co[no] = 0;
        }

        int _schedule_st_::alloc_no() {
            int no = -1;
            if (!this->_freenos.try_dequeue(no)) {
                no = this->_no.fetch_add(1);
                if (no < this->_cap) {
                    return no;
                }

                // need to alloc
                std::unique_lock<std::shared_mutex> lock(this->_mu);
                if (no >= this->_cap) {
                    realloc_schedule();
                }
            }

            return no;
        }

        bool _schedule_st_::realloc_schedule() {
            int grow = 0;
            _co_st_ ** co = 0;

            if (this->_cap == 0) {
                grow = GROWUP_COROUTINE;
                co = (_co_st_ **) malloc(sizeof(_co_st_*) * grow);
            } else {
                grow = (int)(GROWUP_COROUTINE * 1.5f);
                co = (_co_st_ **) realloc(this->_co, sizeof(_co_st_*) * (grow + this->_cap));
            }

            if (co == 0) {
                throw "alloc more coroutine error";
            }

            this->_co = co;
            this->_cap += grow;

            gmem.add(sizeof (_co_st_*) * grow);
            //M_CO_DEBUG_PRINT("schedule_st mem:%d\n", this->_cap * sizeof (_co_st_*));
            return true;
        }

        _co_st_* _schedule_st_::get_co(int64_t no) {
            int n = (int)no;
            return this->_co[n];
        }

        void _memory_st_::add(size_t s) {
            this->_mem += (int)s;
        }

        void _memory_st_::dec(size_t s) {
            this->_mem -= (int)s;
        }

        _schedule_st_ gschedule_st;

        _main_co_st_::~_main_co_st_() {
#ifdef M_PLATFORM_WIN
            if (this->_ctx) {
                ::ConvertFiberToThread();
            }
#endif
        }

        thread_local _main_co_st_ gmainco;

        _memory_st_ gmem;
    }
}