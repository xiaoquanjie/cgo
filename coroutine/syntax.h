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
    struct _cgo_stack_syntax_st_ {
        int _stack = 0;
        const std::function<void()>& _routine;

        _cgo_stack_syntax_st_(int stack, const std::function<void()>& routine);
        _cgo_stack_syntax_st_(const std::function<void()>& routine);
    };

    struct _cgo_syntax_st_ {
        const char* _file = 0;
        int _line = 0;

        _cgo_syntax_st_(const char* f, int l);
        void operator <<(const _cgo_stack_syntax_st_&);
    private:
        _cgo_syntax_st_(const _cgo_syntax_st_&) = delete;
        _cgo_syntax_st_& operator=(const _cgo_syntax_st_&) = delete;
    };

    void cgo_wait(int wait_mil);

    void cgo_procs(int cnt);

    void cgo_core_pool(int cnt);

    void cgo_stop();
}

#define Cgo cgo::_cgo_syntax_st_(__FILE__, __LINE__) << (cgo::_cgo_stack_syntax_st_)
#define CgoWait cgo::cgo_wait
#define CgoProcs cgo::cgo_procs
#define CgoCorePool cgo::cgo_core_pool
#define CgoStop cgo::cgo_stop