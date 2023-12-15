/*----------------------------------------------------------------
// Copyright 2021
// All rights reserved.
//
// author: 404558965@qq.com (xiaoquanjie)
// github: https://github.com/xiaoquanjie/cgo
// Created by xiaoqj on 2023/11/04
//----------------------------------------------------------------*/

#include "syntax.h"

namespace cgo {
    namespace scheduler {
        void schedule_task(const std::function<void()>& routine, int stack, const char* file, int line);
        void schedule_wait(int wait_mil);
        void set_cgo_procs(int cnt);
        void set_cgo_core(int cnt);
        void print_debug_info();
    }

    void _cgo_syntax_st_::operator >>(const std::function<void()>& routine) {
        scheduler::schedule_task(routine, _stack, _file, _line);
    }

    _cgo_syntax_st_& _cgo_syntax_st_::operator >>(const _cgo_stack_st_& stack) {
        this->_stack = stack._stack;
        return *this;
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

    void cgo_print_debug_info() {
        scheduler::print_debug_info();
    }
}