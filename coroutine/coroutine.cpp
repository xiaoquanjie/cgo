/*----------------------------------------------------------------
// Copyright 2021
// All rights reserved.
//
// author: 404558965@qq.com (xiaoquanjie)
// github: https://github.com/xiaoquanjie/cgo
// Created by xiaoqj on 2023/10/31.
//----------------------------------------------------------------*/

#include "structure.h"

namespace cgo {
    namespace coroutine {
        int64_t create(std::function<void()> routine);

        void resume(int64_t co_id);

        int64_t curid() {
            return gmainco._curno;
        }

        int create(void(*routine)(void *), void *data) {
            auto f = std::bind(routine, data);
            return create(f);
        }

        void run(std::function<void()> routine) {
            auto co_id = create(routine);
            resume(co_id);
        }

        int memory() {
            return gmem._mem;
        }
    }
}
