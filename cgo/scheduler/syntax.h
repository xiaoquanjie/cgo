/*----------------------------------------------------------------
// Copyright 2021
// All rights reserved.
//
// author: 404558965@qq.com (xiaoquanjie)
// github: https://github.com/xiaoquanjie/cgo
// Created by xiaoqj on 2023/11/04
//----------------------------------------------------------------*/

#pragma once

#include <functional>

namespace cgo {
    struct _cgo_stack_st_ {
        int _stack = 0;
    };

    struct _cgo_syntax_st_ {
        const char* _file = nullptr;
        int _line = 0;
        int _stack = 0;

        inline _cgo_syntax_st_(const char* f, int l) : _file(f), _line(l) {}
        void operator >>(const std::function<void()>& routine);
        _cgo_syntax_st_& operator >>(const _cgo_stack_st_& stack);

        _cgo_syntax_st_(const _cgo_syntax_st_&) = delete;
        _cgo_syntax_st_& operator=(const _cgo_syntax_st_&) = delete;
    };

    unsigned long long cgo_cur_coid();

    void cgo_resume(unsigned long long co_id);

    void cgo_yield();

    void cgo_wait(int wait_mil);

    void cgo_procs(int cnt);

    void cgo_core(int cnt);

    void cgo_add_loop(const std::function<void()>& f);

    void cgo_default_stack(int stack);

    void cgo_print_debug_info(bool enable = true);
}

// learn from libgo
#undef go
#define go cgo::_cgo_syntax_st_(__FILE__, __LINE__) >>

#undef gostack
#define gostack(size) cgo::_cgo_stack_st_{size} >>

#undef gowait
#define gowait cgo::cgo_wait

#undef gosleep
#define gosleep cgo::cgo_wait

#undef cgoprocs
#define cgoprocs cgo::cgo_procs

#undef cgocore
#define cgocore cgo::cgo_core

#undef cgoloop
#define cgoloop cgo::cgo_add_loop

#undef cgocoid
#define cgocoid cgo::cgo_cur_coid

#undef cgoyield
#define cgoyield cgo::cgo_yield

#undef cgoresume
#define cgoresume cgo::cgo_resume