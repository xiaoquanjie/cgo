//
// Created by xiaoqj on 2023/12/15.
//

#include "coroutine_adapter.h"
#include <assert.h>

#if defined(USE_CGO_COROUTINE)
#include "coroutine.h"
#include "common/macro.h"
#elif defined(USE_MINI_CORO)
#define MINICORO_IMPL
#include "minicoro.h"
#endif

namespace cgo {
    namespace coro_adapter {
#if defined(USE_MINI_CORO)
        struct co_adapter_info {
            std::function<void()> routine;
            std::function<void()> after;
        };

        void minicoro_routine(mco_coro* co) {
            void* data = 0;
            mco_pop(co, &data, sizeof(void*));
            co_adapter_info* info = (co_adapter_info*)mco_get_user_data(co);
            info->routine();
        }
#elif defined(USE_CGO_COROUTINE)
        struct co_adapter_info {
            std::function<void()> after;
            void* volatile data = 0;
        };
#endif

        uint64_t create_co(std::function<void()> routine, int stack, const char* file, int line) {
#if defined(USE_CGO_COROUTINE)
            auto co_id = coroutine::create(routine, stack, file, line);
            co_adapter_info* info = new co_adapter_info;
            coroutine::set_udata(co_id, info);
            return co_id;
#elif defined(USE_MINI_CORO)
            mco_desc desc = mco_desc_init(minicoro_routine, stack);
            co_adapter_info* info = new co_adapter_info;
            info->routine = routine;
            desc.user_data = (void*)info;

            mco_coro* co;
            mco_result res = mco_create(&co, &desc);
            assert(res == MCO_SUCCESS);
            return (uintptr_t)(co);
#else
#pragma message("no coroutine implement")
#endif
        }

        void resume_co(uint64_t co_id, void* data) {
#if defined(USE_CGO_COROUTINE)
            co_adapter_info* info = (co_adapter_info*)coroutine::get_udata(co_id);
            info->data = data;
            auto status = coroutine::resume(co_id);
            switch (status) {
                case COROUTINE_SUSPEND: {
                    if (info->after) {
                        info->after();
                    }
                    break;
                }
                case COROUTINE_DEAD: {
                    delete info;
                    break;
                }
            }
#elif defined(USE_MINI_CORO)
            mco_coro* co = (mco_coro*)uintptr_t(co_id);
            mco_push(co, &data, sizeof(void*));
            mco_resume(co);
            switch (mco_status(co)) {
                case MCO_DEAD: {
                    co_adapter_info* info = (co_adapter_info*)mco_get_user_data(co);
                    delete info;
                    mco_destroy(co);
                    break;
                }
                case MCO_SUSPENDED: {
                    co_adapter_info* info = (co_adapter_info*)mco_get_user_data(co);
                    if (info->after) {
                        info->after();
                    }
                    break;
                }
            }
#else
#pragma message("no coroutine implement")
#endif
        }

        void yield_co(void*& data, const std::function<void()>& after) {
#if defined(USE_CGO_COROUTINE)
            auto co2 = coroutine::curid();
            co_adapter_info* info = (co_adapter_info*)coroutine::get_udata(co2);
            info->after = after;
            coroutine::yield();
            data = info->data;
#elif defined(USE_MINI_CORO)
            auto co2 = mco_running();
            co_adapter_info* info = (co_adapter_info*)mco_get_user_data(co2);
            info->after = after;
            mco_yield(co2);
            auto co = mco_running();
            mco_pop(co, &data, sizeof(void*));
#else
#pragma message("no coroutine implement")
#endif
        }

        void yield_co(void*& data) {
            yield_co(data, nullptr);
        }

        void yield_co() {
            void* data = 0;
            yield_co(data, nullptr);
        }

        void run_co(std::function<void()> routine, int stack, const char* file, int line) {
            auto co_id = create_co(routine, stack, file, line);
            resume_co(co_id, 0);
        }

        uint64_t cur_coid() {
#if defined(USE_CGO_COROUTINE)
            return coroutine::curid();
#elif defined(USE_MINI_CORO)
            auto co = mco_running();
            if (co != 0) {
                return (uintptr_t)co;
            }
            return (uint64_t)-1;
#else
#pragma message("no coroutine implement")
#endif
        }
    }
}