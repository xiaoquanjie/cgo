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
        void schedule_task(const char* file, int line, std::function<void()> routine);
        void schedule_wait(int wait_mil);
        void set_max_procs(int cnt);
        void stop();
    }

    _cgo_syntax_st_::_cgo_syntax_st_(const char* f, int l) : _file(f), _line(l) {}

    void _cgo_syntax_st_::operator <<(std::function<void()> routine) {
        scheduler::schedule_task(_file, _line, routine);
    }

    void cgo_wait(int wait_mil) {
        scheduler::schedule_wait(wait_mil);
    }

    void cgo_procs(int cnt) {
        if (cnt < 1) {
            cnt = 1;
        }
        scheduler::set_max_procs(cnt);
    }

    void cgo_stop() {
        scheduler::stop();
    }
}