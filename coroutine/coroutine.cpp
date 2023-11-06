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
        int64_t create(std::function<void()> routine, const char* file, int line);

        void resume(int64_t co_id);
        void yield();

        int64_t curid() {
            return gmainco._curno;
        }

        void wait(int wait_mil) {
            if (wait_mil <= 0) {
                wait_mil = 1;
            }

            auto co_id = curid();
            if (co_id == -1) {
                return;
            }

            auto timer_id = gtimepool.async_add_timer((uint32_t)wait_mil, [co_id]() {
                resume(co_id);
            });

            if (timer_id == 0) {
                return;
            }

            yield();
        }

        void run(std::function<void()> routine, const char* file, int line) {
            auto co_id = create(routine, file, line);
            resume(co_id);
        }

        void run(std::function<void()> routine) {
            run(routine, nullptr, 0);
        }

        int memory() {
            return gmem._mem;
        }
    }
}
