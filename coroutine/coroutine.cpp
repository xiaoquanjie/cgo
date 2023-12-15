/*----------------------------------------------------------------
// Copyright 2021
// All rights reserved.
//
// author: 404558965@qq.com (xiaoquanjie)
// github: https://github.com/xiaoquanjie/cgo
// Created by xiaoqj on 2023/10/31.
//----------------------------------------------------------------*/

#include "structure.h"
#include "coroutine.h"

namespace cgo {
    namespace coroutine {
        uint64_t create(std::function<void()> routine, int stack, const char* file, int line);

        uint64_t curid() {
            uint64_t id = gcurno;
            if (id == M_INVALID_COROUTINE_ID) {
                return id;
            }
            return id;
        }

        int num() {
            return 0;
        }

        void run(std::function<void()> routine, int stack, const char* file, int line) {
            auto co_id = create(routine, stack, file, line);
            resume(co_id);
        }

        void run(std::function<void()> routine) {
            run(routine, 0, nullptr, 0);
        }

        int memory() {
            return gmem._mem;
        }

        // yield
        void yield() {
            void* data = 0;
            yield(data);
        }
    }
}
