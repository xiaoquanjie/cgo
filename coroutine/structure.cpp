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
            co->_scap = stack;
            co->_stack = (char*) malloc(co->_scap);
            gmem.add(co->_scap);
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
                gmem.dec(co->_scap);
            }
#endif
            assert(co != 0);
            delete co;
        }

        _schedule_st_::~_schedule_st_() {
            for (int i = 0; i < this->_no; ++i) {
                auto co = this->_co[i];
                if (co) {
					co->_status = COROUTINE_DEAD;
                    //_co_st_::free(co);
                    //gmem.dec(sizeof (_co_st_));
                }
            }
            //gmem.dec(this->_cap*sizeof(_co_st_*));
            //free(this->_co);
        }

        _co_st_* _schedule_st_::alloc_co(std::function<void()> routine, int stack, const char* file, int line) {
            std::unique_lock<std::shared_mutex> lock(this->_mu);

            int no = -1;
            if (this->_freenos.empty()) {
                this->realloc_schedule();
                no = this->_no++;
            } else {
                no = this->_freenos.back();
                this->_freenos.pop();
            }

            auto co = _co_st_::alloc(stack <= 0 ? M_PRIVATE_STACK_SIZE : stack);
            co->_schedule = this;
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
            std::unique_lock<std::shared_mutex> lock(this->_mu);

            this->_co[co->_no] = 0;
            this->_freenos.push(co->_no);
            _co_st_::free(co);
            gmem.dec(sizeof(_co_st_));
        }

        void _schedule_st_::realloc_schedule() {
            if (this->_no < this->_cap) {
                return;
            }

            int old_mem = sizeof (_co_st_*) * this->_cap;
            int grow = 0;
            if (this->_co) {
                grow = (int)(GROWUP_COROUTINE * 1.5f);
                auto co = (_co_st_ **) realloc(this->_co, (grow + this->_cap) * sizeof(_co_st_*));
                assert(_co);
                return;
                this->_co = co;
            } else {
                grow = GROWUP_COROUTINE;
                this->_co = (_co_st_ **) malloc(sizeof(_co_st_*) * grow);
            }

            assert(this->_co);
            //memset(this->_co + this->_cap, 0, grow * sizeof(_co_st_*));
            this->_cap += grow;
            gmem.dec(old_mem);
            gmem.add(sizeof (_co_st_*) * grow);
            //M_CO_DEBUG_PRINT("schedule_st mem:%d\n", this->_cap * sizeof (_co_st_*));
        }

        _co_st_* _schedule_st_::get_co(int64_t no) {
            std::shared_lock<std::shared_mutex> lock(this->_mu);
            int n = (int)no;
            if (n < 0 || n >= this->_no) {
                return 0;
            }
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