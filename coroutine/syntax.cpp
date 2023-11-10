/*----------------------------------------------------------------
// Copyright 2021
// All rights reserved.
//
// author: 404558965@qq.com (xiaoquanjie)
// github: https://github.com/xiaoquanjie/cgo
// Created by xiaoqj on 2023/11/04
//----------------------------------------------------------------*/

#include "syntax.h"
#include "scheduler.h"

namespace cgo {
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
        scheduler::set_cgo_procs(cnt > 0 ? cnt : 1);
    }

    void cgo_core_pool(int cnt) {
        scheduler::set_core_pool(cnt > 0 ? cnt : 1);
    }

    void cgo_stop() {
        scheduler::stop();
    }
}