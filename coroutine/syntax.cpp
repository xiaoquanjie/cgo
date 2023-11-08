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
        void set_core_pool(int cnt);
        void stop();
    }

    _cgo_stack_syntax_st_::_cgo_stack_syntax_st_(int stack, const std::function<void()>& routine)
        : _stack(stack), _routine(routine) {}

    _cgo_stack_syntax_st_::_cgo_stack_syntax_st_(const std::function<void()>& routine)
        : _routine(routine), _stack(0) {}

    _cgo_syntax_st_::_cgo_syntax_st_(const char* f, int l) : _file(f), _line(l) {}

    void _cgo_syntax_st_::operator <<(const _cgo_stack_syntax_st_& st) {
        scheduler::schedule_task(st._routine, st._stack, _file, _line);
    }

    void cgo_wait(int wait_mil) {
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