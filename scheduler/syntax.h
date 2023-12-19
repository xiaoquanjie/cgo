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
        const char* _file = 0;
        int _line = 0;
        int _stack = 0;

        inline _cgo_syntax_st_(const char* f, int l) : _file(f), _line(l) {}
        void operator >>(const std::function<void()>& routine);
        _cgo_syntax_st_& operator >>(const _cgo_stack_st_& stack);
    private:
        _cgo_syntax_st_(const _cgo_syntax_st_&) = delete;
        _cgo_syntax_st_& operator=(const _cgo_syntax_st_&) = delete;
    };

    void cgo_wait(int wait_mil);

    void cgo_procs(int cnt);

    void cgo_core(int cnt);

    void cgo_print_debug_info();
}

// learn from libgo
#undef go
#define go cgo::_cgo_syntax_st_(__FILE__, __LINE__) >>

#undef gostack
#define gostack(size) cgo::_cgo_stack_st_{size} >>

#undef gowait
#define gowait(mil) cgo::cgo_wait(mil)

#undef cgoprocs
#define cgoprocs(cnt) cgo::cgo_procs(cnt)

#undef cgocore
#define cgocore(cnt) cgo::cgo_core(cnt)