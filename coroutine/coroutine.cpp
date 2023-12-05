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
            uint64_t id = gmainco._curno;
            if (id == M_INVALID_COROUTINE_ID) {
                return id;
            }
            return id;
        }

        bool suspend_wait(uint64_t co_id) {
            auto co = _co_st_::get_co(co_id);
            if (!co) {
                assert(false);
                return false;
            }
            while (co->_status != COROUTINE_SUSPEND && co->_status == COROUTINE_DEAD) {}
            return true;
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
            yield(0);
        }
    }
}
