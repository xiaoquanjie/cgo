/*----------------------------------------------------------------
// Copyright 2021
// All rights reserved.
//
// author: 404558965@qq.com (xiaoquanjie)
// github: https://github.com/xiaoquanjie/cgo
// Created by xiaoqj on 2023/11/04
//----------------------------------------------------------------*/

#include "syntax.h"
#include "task.h"

namespace cgo {
    namespace coroutine {
        void schedule_task(const char* file, int line, std::function<void()> routine);

        _cgo_syntax_st_::_cgo_syntax_st_(const char* f, int l) : _file(f), _line(l) {}

        void _cgo_syntax_st_::operator <<(std::function<void()> routine) {
            schedule_task(_file, _line, routine);
        }
    }
}