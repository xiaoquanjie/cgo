//
// Created by xiaoqj on 2023/12/15.
//

#include "coroutine_adapter.h"
#include <assert.h>

#if defined(USE_CGO_COROUTINE)
#include "coroutine.h"
#elif defined(USE_MINI_CORO)
#define MINICORO_IMPL
#include "minicoro.h"
#endif

namespace cgo {
    namespace coro_adapter {
#if defined(USE_MINI_CORO)
        void minicoro_routine(mco_coro* co) {
            void* data = 0;
            mco_pop(co, &data, sizeof(void*));
            std::function<void()>* routine = (std::function<void()>*) mco_get_user_data(co);
            (*routine)();
        }
#endif

        uint64_t create_co(std::function<void()> routine, int stack, const char* file, int line) {
#if defined(USE_CGO_COROUTINE)
            return coroutine::create(routine, stack, file, line);
#elif defined(USE_MINI_CORO)
            mco_desc desc = mco_desc_init(minicoro_routine, stack);
            desc.user_data = (void*)new std::function<void()>(routine);
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
            coroutine::resume(co_id, data);
#elif defined(USE_MINI_CORO)
            mco_coro* co = (mco_coro*)uintptr_t(co_id);
            mco_push(co, &data, sizeof(void*));
            mco_resume(co);
            if (mco_status(co) == MCO_DEAD) {
                std::function<void()>* routine = (std::function<void()>*)(co->user_data);
                delete routine;
                mco_destroy(co);
            }
#else
#pragma message("no coroutine implement")
#endif
        }

        void yield_co(void*& data) {
#if defined(USE_CGO_COROUTINE)
            coroutine::yield(data);
#elif defined(USE_MINI_CORO)
            auto co2 = mco_running();
            mco_yield(co2);
            auto co = mco_running();
            mco_pop(co, &data, sizeof(void*));
#else
#pragma message("no coroutine implement")
#endif
        }

        void yield_co() {
            void* data = 0;
            yield_co(data);
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
            if (co == 0) {
                return (uintptr_t)co;
            }
            return (uint64_t)-1;
#else
#pragma message("no coroutine implement")
#endif
        }
    }
}