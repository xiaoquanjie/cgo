//
// Created by xiaoqj on 2023/12/15.
//

#include "coroutine_adapter.h"
#include <assert.h>
#include <atomic>

#if defined(USE_CGO_COROUTINE)
#include "coroutine.h"
#include "common/macro.h"
#elif defined(USE_MINI_CORO)
#define MINICORO_IMPL
#include "minicoro.h"
#endif

#if defined(USE_CGO_COROUTINE)
#define M_YIELD_CO(co_id) coroutine::yield(co_id)
#define M_GET_MULTI_INFO(co_id) (co_multiplexing_info*)coroutine::get_udata(co_id)
#define M_CUR_COID() coroutine::curid()
#define M_RESUME_CO(co_id) coroutine::resume(co_id)
#define M_DELETE_CO(co_id, info) delete info;
#elif defined(USE_MINI_CORO)
#define M_YIELD_CO(co_id) {mco_coro* co = (mco_coro*)uintptr_t(co_id); mco_yield(co);}
#define M_GET_MULTI_INFO(co_id) (co_multiplexing_info*)mco_get_user_data((mco_coro*)uintptr_t(co_id))
#define M_CUR_COID() (uintptr_t)mco_running()
#define M_RESUME_CO(co_id) (mco_resume((mco_coro*)uintptr_t(co_id)), mco_status((mco_coro*)uintptr_t(co_id)))
#define COROUTINE_SUSPEND MCO_SUSPENDED
#define COROUTINE_DEAD MCO_DEAD
#define M_DELETE_CO(co_id, info) delete info; mco_destroy((mco_coro*)uintptr_t(co_id));
#endif

namespace cgo {
    namespace coro_adapter {
        enum {
            WaitSignal = 1 << 0,
            ActiveSignal = 1 << 1,
        };

        struct co_multiplexing_info {
#if defined(USE_MINI_CORO)
            std::function<void()> routine;
#endif
            std::function<void()> after;
            volatile bool hook;
            const char* volatile file;
            volatile unsigned short line;
            void* volatile data;
            std::atomic_char signal;   // 第0位为1表示等待状态， 第1位为1表示激活状态，第2位为1表示加锁状态

            void init() {
                after = nullptr;
                hook = false;
                file = 0;
                line = 0;
                data = 0;
                signal = 0;
            }
            inline void lock() {
                char c;
                for (;;) {
                    c = signal.load(std::memory_order_relaxed);
                    if (c & (1 << 2))
                        continue;
                    if (signal.compare_exchange_weak(c, c | (1 << 2), std::memory_order_relaxed)) {
                        break;
                    }
                }
            }
            inline void unlock() {
                char c = signal.load(std::memory_order_relaxed);
                if (c) {
                    signal.store(c & ~(1<<2), std::memory_order_relaxed);
                }
            }
            inline void set_signal(int s, bool cancel) {
                char c = signal.load(std::memory_order_relaxed);
                if (cancel) {
                    signal.store(c & ~s, std::memory_order_relaxed);
                } else {
                    signal.store(c | s, std::memory_order_relaxed);
                }
            }
        };

#if defined(USE_MINI_CORO)
        void minicoro_routine(mco_coro* co) {
            co_multiplexing_info* info = (co_multiplexing_info*)mco_get_user_data(co);
            info->routine();
        }
#endif

        // 曾考虑过这里是否要使用协程池，但经过测试发现协程池也只能提升10%左右的性能(只测试了linux下的性能)，因此暂时没有动机添加协程池
        uint64_t create_co(const std::function<void()>& routine, int stack, const char* file, int line) {
            uint64_t co_id = 0;
            auto info = new co_multiplexing_info;
            info->init();
            info->file = file;
            info->line = (unsigned short)line;

#if defined(USE_CGO_COROUTINE)
            co_id = coroutine::create(routine, stack);
            coroutine::set_udata(co_id, info);
#elif defined(USE_MINI_CORO)
            mco_desc desc = mco_desc_init(minicoro_routine, stack);
            info->routine = routine;
            desc.user_data = (void*)info;

            mco_coro* co;
            mco_result res = mco_create(&co, &desc);
            assert(res == MCO_SUCCESS);
            co_id = (uintptr_t)(co);
#else
#pragma message("no coroutine implement")
#endif
            return co_id;
        }

        void resume_co(uint64_t co_id, void* data) {
            auto info = M_GET_MULTI_INFO(co_id);
            info->data = data;
            auto status = M_RESUME_CO(co_id);
            switch (status) {
                case COROUTINE_SUSPEND: {
                    info->unlock();
                    if (info->after) info->after();
                    break;
                }
                case COROUTINE_DEAD: {
                    M_DELETE_CO(co_id, info);
                    break;
                }
            }
        }

        void co_wait_signal(void*& data) {
            auto co_id = M_CUR_COID();
            auto info = M_GET_MULTI_INFO(co_id);
            info->lock();
            info->after = nullptr;
            char sig = info->signal;
            assert((sig & WaitSignal) == 0);
            if (sig & ActiveSignal) {
                // 如果状态是激活的, 取消状态
                info->signal.store(sig & ~ActiveSignal, std::memory_order_relaxed);
                data = info->data;
                info->unlock();
            } else {
                // 如果状态是未激活
                info->signal.store(sig | WaitSignal, std::memory_order_relaxed);
                // 挂起协程
                M_YIELD_CO(co_id);
                data = info->data;
            }
        }

        void co_post_signal(uint64_t co_id, void* data) {
            auto info = M_GET_MULTI_INFO(co_id);
            info->lock();
            char sig = info->signal;
            assert((sig & ActiveSignal) == 0);
            if (sig & WaitSignal) {
                // 如果状态是等待, 取消状态
                info->signal.store(sig & ~WaitSignal, std::memory_order_relaxed);
                info->unlock();
                // 唤醒协程
                resume_co(co_id, data);
            } else {
                info->data = data;
                info->signal.store(sig | ActiveSignal, std::memory_order_relaxed);
                info->unlock();
            }
        }

        void yield_co(void*& data, const std::function<void()>& after) {
            auto co_id = M_CUR_COID();
            auto info = M_GET_MULTI_INFO(co_id);
            info->after = after;
            M_YIELD_CO(co_id);
            data = info->data;
        }

        void yield_co(void*& data) {
            yield_co(data, nullptr);
        }

        void yield_co() {
            void* data = 0;
            yield_co(data, nullptr);
        }

        void run_co(const std::function<void()>& routine, int stack, const char* file, int line) {
            auto co_id = create_co(routine, stack, file, line);
            resume_co(co_id, 0);
        }

        uint64_t cur_coid() {
            auto co_id = M_CUR_COID();
            if (co_id == 0) {
                return (uint64_t)-1;
            }
            return co_id;
        }

        void co_hook(bool enable) {
            auto co_id = M_CUR_COID();
            auto info = M_GET_MULTI_INFO(co_id);
            info->hook = enable;
        }

        bool co_hook() {
            auto co_id = M_CUR_COID();
            auto info = M_GET_MULTI_INFO(co_id);
            return info->hook;
        }
    }
}