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
        int64_t create(std::function<void()> routine, int stack, const char* file, int line);

        int64_t curid() {
            int64_t id = gmainco._curno;
            if (id == -1) {
                return id;
            }

#ifdef M_PLATFORM_WIN
            int64_t workid = gworkid;
            id = (workid << 32) + id;
#endif
            return id;
        }

#ifdef M_PLATFORM_WIN
        void decode_coid(int64_t co_id, int32_t& work_id, int64_t& real_id) {
            work_id = co_id >> 32;
            real_id = co_id & 0xFFFFFFFF;
        }

#endif
        int64_t real_curid() {
            return gmainco._curno;
        }

        bool suspend_wait(int64_t co_id) {
            auto co = gschedule_st.get_co(co_id);
            if (!co) {
                assert(false);
                return false;
            }
            while (co->_status != COROUTINE_SUSPEND && co->_status == COROUTINE_DEAD) {}
            return true;
        }

        int num() {
            auto n = gschedule_st._no - gschedule_st._freenos.size_approx();
            return (int)n;
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
    }
}
