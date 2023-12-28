/*----------------------------------------------------------------
// Copyright 2021
// All rights reserved.
//
// author: 404558965@qq.com (xiaoquanjie)
// github: https://github.com/xiaoquanjie/cgo
// Created by xiaoqj on 2023/11/04
//----------------------------------------------------------------*/

#include "syntax.h"
#include <stdint.h>

namespace cgo {
    namespace scheduler {
        uint64_t cur_coid();
        void schedule_task(const std::function<void()>& routine, int stack, const char* file, int line);
        void schedule_yield();
        void schedule_yield(void*& data, const std::function<void()>& after);
        void schedule_wait(int wait_mil);
        void schedule_co(uint64_t co_id, void*);
        void set_cgo_procs(int cnt);
        void set_cgo_core(int cnt);
        void cgo_add_loop(const std::function<void()>& f);
        void print_debug_info();
    }

    void _cgo_syntax_st_::operator >>(const std::function<void()>& routine) {
        scheduler::schedule_task(routine, _stack, _file, _line);
    }

    _cgo_syntax_st_& _cgo_syntax_st_::operator >>(const _cgo_stack_st_& stack) {
        this->_stack = stack._stack;
        return *this;
    }

    unsigned long long cgo_cur_coid() {
        return scheduler::cur_coid();
    }

    void cgo_resume(unsigned long long co_id) {
        scheduler::schedule_co(co_id, 0);
    }

    void cgo_yield() {
        scheduler::schedule_yield();
    }

    void cgo_yield(void*& data, const std::function<void()>& after) {
        scheduler::schedule_yield(data, after);
    }

    void cgo_wait(int wait_mil) {
        if (wait_mil <= 0) {
            return;
        }
        scheduler::schedule_wait(wait_mil);
    }

    void cgo_procs(int cnt) {
        scheduler::set_cgo_procs(cnt);
    }

    void cgo_core(int cnt) {
        scheduler::set_cgo_core(cnt);
    }

    void cgo_add_loop(const std::function<void()>& f) {
        scheduler::cgo_add_loop(f);
    }

    void cgo_print_debug_info() {
        scheduler::print_debug_info();
    }
}