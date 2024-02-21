/*----------------------------------------------------------------
// Copyright 2021
// All rights reserved.
//
// author: 404558965@qq.com (xiaoquanjie)
// github: https://github.com/xiaoquanjie/cgo
// Created by xiaoqj on 2023/11/04
//----------------------------------------------------------------*/

#include "scheduler.h"
#include "common/macro.h"
#include "common/time_pool.h"
#include "common/print.h"
#include "common/concurrentqueue.h"
#include "common/work_steal_queue.hpp"
#include "common/semaphore.h"
#include <memory>
#include <thread>
#include <shared_mutex>
#include <vector>
#include <algorithm>
#include <string>

namespace cgo {
    namespace coro_adapter {
        uint64_t create_co(const std::function<void()>& routine, int stack, const char* file, int line);
        void resume_co(uint64_t co_id);
        void co_wait_signal(void*& data);
        void co_post_signal(uint64_t co_id, void* data);
        void yield_co(void*& data);
        void yield_co();
        void run_co(const std::function<void()>& routine, int stack, const char* file, int line);
        uint64_t cur_coid();
        void co_hook(bool enable);
        bool co_hook();
    }

    namespace scheduler {
        struct co_pool_item {
            uint64_t _co_id;
            routine_fn _fn;
        };
        struct _schedule_task_st_ {
            enum {
                RunCo = 0,
                ResumeCo = 1,
                PostSignal = 2,
            };
            unsigned char _type;
        };
        struct _schedule_comm_task_st_ : public _schedule_task_st_ {
            uint64_t _co_id;
            void* _data;
        };
        struct _schedule_newco_task_st_ : public _schedule_task_st_{
            routine_fn _fn;
            int _statck;
            const char* _f;
            int _l;
        };

        using concurrent_task_queue_type = moodycamel::ConcurrentQueue<_schedule_task_st_*>;
        using wsq_task_queue_type = WorkStealingQueue<_schedule_task_st_*>;
        using concurrent_loop_queue_type = moodycamel::ConcurrentQueue<routine_fn>;

        struct _schedule_base_queue_st_ {
            using time_point = std::chrono::time_point<std::chrono::steady_clock>;
            time_point _clock;

            inline void record() {
                if (this->_clock.time_since_epoch().count() == 0) {
                    this->_clock = std::chrono::steady_clock::now();
                }
            }

            inline void unrecord() {
                if (this->size() == 0) {
                    this->_clock = time_point();
                }
            }

            inline uint64_t delay_delta() {
                // copy old time
                auto old_time = this->_clock;
                if (!this->is_init()
                    || this->size() == 0
                    || old_time.time_since_epoch().count() == 0) {
                    return 0;
                }

                auto now = std::chrono::steady_clock::now();
                auto expire = (std::chrono::duration_cast<std::chrono::microseconds>(now - old_time)).count();
                return expire;
            }

            // 在规定时间内没有清空队列，就会被判定为队列堆积
            inline bool delay() {
                auto delta = delay_delta();
                if (delta >= M_QUEUE_DELAY_TIME) {
                    return true;
                }
                return false;
            }

            inline bool secondclass_delay() {
                auto delta = delay_delta();
                if (delta >= M_QUEUE_SECONDCLASS_DELAY_TIME) {
                    return true;
                }
                return false;
            }

            inline bool emergency() {
                auto delta = delay_delta();
                // over 10 seconds
                if (delta >= 10 * 1000 * 1000) {
                    return true;
                }
                return false;
            }

            virtual ~_schedule_base_queue_st_() {}
            virtual bool is_init() { return true; }
            virtual std::size_t size() = 0;
            virtual void enqueue(_schedule_task_st_*) = 0;
            virtual bool try_dequeue(_schedule_task_st_*&) = 0;
            virtual void steal(int32_t count, _schedule_base_queue_st_* to) = 0;
        };

        struct _schedule_local_queue_st_ : public _schedule_base_queue_st_ {
        private:
            bool _init = false;
            wsq_task_queue_type* _queue;
        public:
            _schedule_local_queue_st_();
            ~_schedule_local_queue_st_();
            bool is_init() override;
            inline std::size_t size() override;
            inline void enqueue(_schedule_task_st_*) override;
            bool try_dequeue(_schedule_task_st_*&) override;
            void steal(int32_t count, _schedule_base_queue_st_* to) override;

            _schedule_local_queue_st_(const _schedule_local_queue_st_&) = delete;
            _schedule_local_queue_st_& operator=(const _schedule_local_queue_st_&) = delete;
        };

        struct _schedule_global_queue_st_ : public _schedule_base_queue_st_ {
        private:
            //std::atomic_int _cnt;
            concurrent_task_queue_type* _queue;
        public:
            _schedule_global_queue_st_();
            ~_schedule_global_queue_st_();
            inline std::size_t size() override;
            inline void enqueue(_schedule_task_st_*) override;
            bool try_dequeue(_schedule_task_st_*&) override;
            void steal(int32_t count, _schedule_base_queue_st_* to) override;
        };

        struct _co_pool_st_;
        extern thread_local _co_pool_st_* volatile g_local_cpool;

        struct _co_pool_st_ {
            std::vector<co_pool_item*> _items;
            void run(_schedule_newco_task_st_* task, int stack) {
                if (_items.empty()) {
                    auto item = new co_pool_item;
                    item->_fn.swap(task->_fn);
                    item->_co_id = coro_adapter::create_co([item]{
                        void* tmp;
                        for (;;) {
                            if (!item->_fn) break;
                            item->_fn();
                            if (!g_local_cpool->recycle_item(item)) {
                                break;
                            }
                            coro_adapter::co_wait_signal(tmp);
                        }
                        // M_CO_DEBUG_PRINT("[cgo_debug] release co_pool_item\n");
                    }, stack, 0, 0);
                    coro_adapter::resume_co(item->_co_id);
                } else {
                    auto item = _items.back();
                    _items.pop_back();
                    item->_fn.swap(task->_fn);
                    coro_adapter::co_post_signal(item->_co_id, 0);
                }
            }
            bool recycle_item(co_pool_item* item) {
                if (_items.size() >= 100) {
                    delete item;
                    return false;
                }
                _items.push_back(item);
                return true;
            }
        };

        struct _schedule_thread_st_ {
            int _work_id = 0;
            std::thread* _thr = 0;
            _scheduler_st_* _scheduler = 0;
            _schedule_base_queue_st_* _local_tqueue = 0;
            _co_pool_st_ _local_cpool;
            std::uint64_t _task_op_cnt = 0;
            volatile bool _stop = false;
            Semaphore _sem;
            _schedule_thread_st_() = default;
            ~_schedule_thread_st_() = default;
            static _schedule_thread_st_* create(_scheduler_st_* s,
                                                int wid,
                                                _schedule_base_queue_st_* fromq);
            void on_init();
            void on_release();
            void on_run();
            void join();
            inline void wait();
            inline void resume(_schedule_base_queue_st_* from = 0);
            inline bool is_stop();
            void stop();
            _schedule_thread_st_(const _schedule_thread_st_&) = delete;
            _schedule_thread_st_& operator=(const _schedule_thread_st_&) = delete;
        };

        struct _schedule_watch_thread_st_ {
            std::thread _thr;
            _schedule_watch_thread_st_(void(*f)()) : _thr(f) {}
            inline void join() {
                this->_thr.join();
            }
        };
        using schedule_thread_st_type = _schedule_thread_st_*;

        struct _schedule_dead_thread_st_ {
            schedule_thread_st_type _thr;
            int64_t _time;
        };

        struct _scheduler_st_ {
            _schedule_global_queue_st_* _global_tqueue;
            std::list<schedule_thread_st_type> _work_threads; // 正在工作的线程
            std::list<schedule_thread_st_type> _idle_threads; // 空闲的线程
            std::vector<_schedule_dead_thread_st_> _dead_threads;
            int generate_work_id = 1;
            std::shared_mutex _thrs_mu;
            _schedule_watch_thread_st_ _watcher;

            std::atomic_bool _stop = false;
            std::atomic_int _max_thr_cnt = 0;   // max thread count
            std::atomic_int _core_thr_cnt = 1;  // core thread count
            std::atomic_int _thr_cnt = 0;       // current thread count
            std::atomic_uint64_t _task_op_cnt = 0;

            //async_time_pool _time_pool;
            integer_async_time_pool _time_pool;
            std::vector<routine_fn> _loops;
            concurrent_loop_queue_type _loops_buf;
            bool _print_debug_info = false;
            int _default_stack = M_PRIVATE_STACK_SIZE;

            _scheduler_st_();

            ~_scheduler_st_();

            void stop();

            bool run();

            bool can_start();

            bool start_thread(_schedule_base_queue_st_* newq);

            void steal_task(_schedule_base_queue_st_* from, _schedule_base_queue_st_* to, int32_t count);

            void steal_task(_schedule_thread_st_* st);

            void get_work_threads(std::vector<schedule_thread_st_type>& thrs);

            void get_work_idle_threads(std::vector<schedule_thread_st_type>&,
                                       std::vector<schedule_thread_st_type>&);

            void add_idle_thread(_schedule_thread_st_* st);

            void remove_idle_thread(_schedule_thread_st_* st);

            void delete_idle_thread(_schedule_thread_st_* st, int64_t now);

            void add_work_thread(_schedule_thread_st_* st);

            void print_debug_info(bool enable = true);
        protected:
            static void work_func(_schedule_thread_st_* st);

            static void watch_func();

            static void notify_wait(integer_async_time_pool::Node* node);
        };

        _schedule_local_queue_st_::_schedule_local_queue_st_() {
            _queue = new wsq_task_queue_type(M_MAX_LOCAL_TASK_QUEUE);
        }

        _schedule_local_queue_st_::~_schedule_local_queue_st_() {
            while (true) {
                auto opt = _queue->pop();
                if (opt == std::nullopt) {
                    break;
                }
                delete opt.value();
            }
            delete _queue;
        }

        bool _schedule_local_queue_st_::is_init() {
            return _init;
        }

        inline std::size_t _schedule_local_queue_st_::size() {
            return _queue->size();
        }

        inline void _schedule_local_queue_st_::enqueue(_schedule_task_st_* task) {
            // Only the owner thread can insert an item to the queue.
            _queue->push(task);
            record();
        }

        bool _schedule_local_queue_st_::try_dequeue(_schedule_task_st_*& task) {
            auto opt = _queue->pop();
            if (opt == std::nullopt) {
                return false;
            }

            task = opt.value();
            unrecord();
            _init = true;
            return true;
        }

        void _schedule_local_queue_st_::steal(int32_t count, _schedule_base_queue_st_* to) {
            if (!_init) {
                return;
            }
            while (count > 0) {
                count--;
                auto opt = _queue->steal();
                if (opt == std::nullopt) {
                    break;
                }
                to->enqueue(opt.value());
            }
            unrecord();
        }

        _schedule_global_queue_st_::_schedule_global_queue_st_() {
            _queue = new concurrent_task_queue_type;
            //_cnt = 0;
        }

        _schedule_global_queue_st_::~_schedule_global_queue_st_() {
            delete _queue;
        }

        inline std::size_t _schedule_global_queue_st_::size() {
            return this->_queue->size_approx();
        }

        inline void _schedule_global_queue_st_::enqueue(_schedule_task_st_* task) {
            // 这也是一个瓶颈
            assert(_queue->enqueue(task));
            record();
        }

        bool _schedule_global_queue_st_::try_dequeue(_schedule_task_st_*& task) {
            auto ret = _queue->try_dequeue(task);
            if (ret) {
                unrecord();
            }
            return ret;
        }

        void _schedule_global_queue_st_::steal(int32_t count, _schedule_base_queue_st_* to) {
            while (count > 0) {
                count--;
                _schedule_task_st_* task;
                if (!_queue->try_dequeue(task)) {
                    break;
                }
                to->enqueue(task);
            }
            unrecord();
        }

        //////////////////////////////////////////////////////////////
        _schedule_base_queue_st_* g_global_tqueue = scheduler_inst()._global_tqueue;
        thread_local _schedule_base_queue_st_* volatile g_local_tqueue = g_global_tqueue;
        thread_local _co_pool_st_* volatile g_local_cpool = 0;

        //////////////////////////////////////////////////////////////

        _schedule_thread_st_* _schedule_thread_st_::create(_scheduler_st_* s,
                                                           int wid,
                                                           _schedule_base_queue_st_* fromq) {
            auto st = new _schedule_thread_st_;
            st->_work_id = wid;
            st->_local_tqueue = new _schedule_local_queue_st_;
            st->_scheduler = s;

            if (fromq) {
                s->steal_task(fromq, st->_local_tqueue, 0);
            }

            return st;
        }

        void _schedule_thread_st_::on_init() {
            M_CO_DEBUG_PRINT("[cgo debug] start working thread:%d\n", this->_work_id);
            g_local_tqueue = this->_local_tqueue;
        }

        void _schedule_thread_st_::on_release() {
            M_CO_DEBUG_PRINT("[cgo debug] quit working thread:%d\n", this->_work_id);

            g_local_tqueue = nullptr;
            g_local_cpool = nullptr;

            if (this->_local_tqueue) {
                if (!_scheduler->_stop) {
                    if (this->_local_tqueue->size() > 0) {
                        assert(false);
                    }
                }
                delete this->_local_tqueue;
                this->_local_tqueue = nullptr;
            }

            if (this->_thr) {
                delete _thr;
                _thr = nullptr;
            }

            for (auto item : this->_local_cpool._items) {
                item->_fn = 0;
                coro_adapter::co_post_signal(item->_co_id, 0);
                delete item;
            }

            // delete self
            delete this;
        }

        void _schedule_thread_st_::on_run() {
            uint64_t task_op_cnt = 0;
            for (;;) {
                if (this->is_stop()) {
                    break;
                }

                // run local task
                _schedule_task_st_* task;
                while (this->_local_tqueue->try_dequeue(task)) {
                    task_op_cnt++;
                    g_local_tqueue = this->_local_tqueue;
                    g_local_cpool = &this->_local_cpool;

                    switch (task->_type) {
                        case _schedule_task_st_::RunCo: {
                            auto ntask = (_schedule_newco_task_st_*)task;
                            if (ntask->_statck <= this->_scheduler->_default_stack) {
                                this->_local_cpool.run(ntask, this->_scheduler->_default_stack);
                            } else {
                                coro_adapter::run_co(ntask->_fn, ntask->_statck, ntask->_f, ntask->_l);
                            }
                            delete ntask;
                            break;
                        }
                        case _schedule_task_st_::ResumeCo: {
                            auto ctask = (_schedule_comm_task_st_*)task;
                            coro_adapter::resume_co(ctask->_co_id);
                            delete ctask;
                            break;
                        }
                        case _schedule_task_st_::PostSignal: {
                            auto ctask = (_schedule_comm_task_st_*)task;
                            coro_adapter::co_post_signal(ctask->_co_id, ctask->_data);
                            delete ctask;
                            break;
                        }
                    }
                    if (this->is_stop()) {
                        break;
                    }
                }

                if (this->_local_tqueue->size() == 0) {
                    // steal from the other queue
                    this->_scheduler->steal_task(this);
                    if (this->_local_tqueue->size() == 0) {
                        break;
                    }
                }
            }

            if (task_op_cnt != 0) {
                // add task op count
                this->_scheduler->_task_op_cnt += task_op_cnt;
                this->_task_op_cnt += task_op_cnt;
            }
        }

        void _schedule_thread_st_::join() {
            if (this->_thr) {
                this->resume();
                this->_thr->join();
            }
        }

        inline void _schedule_thread_st_::wait() {
            this->_sem.wait();
        }

        inline void _schedule_thread_st_::resume(_schedule_base_queue_st_* from) {
            if (from) {
                this->_scheduler->steal_task(from, this->_local_tqueue, 0);
            }
            this->_sem.post();
        }

        inline bool _schedule_thread_st_::is_stop() {
            return this->_stop || this->_scheduler->_stop;
        }

        void _schedule_thread_st_::stop() {
            if (this->_stop) {
                return;
            }

            this->_stop = true;
            this->join();

            this->on_release();
        }

        _scheduler_st_::_scheduler_st_() : _watcher(&_scheduler_st_::watch_func), _time_pool(&notify_wait, 1800) {
            _max_thr_cnt = (int)(std::thread::hardware_concurrency() * M_MAX_PROCS_FACTOR);
            _core_thr_cnt = (int)(_max_thr_cnt * M_CORE_POOL_FACTOR);
            _global_tqueue = new _schedule_global_queue_st_;
            std::atexit(cgo::scheduler::cgo_stop);
        }

        _scheduler_st_::~_scheduler_st_() {
            this->stop();
            // don't have to delete _global_tasks, cause of the moodycamel::details::ThreadExitNotifier
            //delete _global_tasks;
        }

        void _scheduler_st_::stop() {
            if (this->_stop) {
                return;
            }

            this->_stop = true;

            // stop watch thread
            this->_watcher.join();

            // stop working threads
            std::vector<schedule_thread_st_type> work_thrs;
            std::vector<schedule_thread_st_type> idle_thrs;
            get_work_idle_threads(work_thrs, idle_thrs);

            for (auto thr : work_thrs) {
                thr->stop();
            }
            for (auto thr : idle_thrs) {
                thr->stop();
            }

            // clear
            this->_work_threads.clear();
            this->_idle_threads.clear();
        }

        bool _scheduler_st_::run() {
            this->_time_pool.update();
            for (auto& f : this->_loops) {
                f();
            }
            routine_fn f;
            while (this->_loops_buf.try_dequeue(f)) {
                f();
                this->_loops.push_back(f);
            }
            return true;
        }

        bool _scheduler_st_::can_start() {
            if (_thr_cnt >= _max_thr_cnt) {
                return false;
            }
            return true;
        }

        bool _scheduler_st_::start_thread(_schedule_base_queue_st_* fromq) {
            if (!can_start()) {
                return false;
            }

            this->_thrs_mu.lock();
            if (!can_start()) {
                this->_thrs_mu.unlock();
                return false;
            }

            this->_thr_cnt++;
            this->_thrs_mu.unlock();

            auto st = _schedule_thread_st_::create(this, generate_work_id++, fromq);
            std::function<void()> work = std::bind(&_scheduler_st_::work_func, st);
            st->_thr = new std::thread(work);
            this->add_work_thread(st);
            return true;
        }

        void _scheduler_st_::steal_task(_schedule_base_queue_st_* from, _schedule_base_queue_st_* to, int32_t count) {
            if (!from) {
                return;
            }

            auto has = from->size();
            if (from != this->_global_tqueue) {
                if (/*has <= 1 ||*/ !from->delay()) {
                    return;
                }
            } else if (has <= 0) {
                return;
            }

            if (count != 0) {
                from->steal(count, to);
            } else {
                has = has % 2 != 0 ? (has / 2) + 1 : has / 2;
                int32_t need = M_MAX_LOCAL_TASK_QUEUE - (int32_t)to->size();
                if (need <= 0) {
                    return;
                }

                // (std::min) for windows
                need = (std::min)(need, (int32_t)has);
                from->steal(need, to);
            }
        }

        void _scheduler_st_::steal_task(_schedule_thread_st_* st) {
            // steal from global first
            // max local task queue: M_MAX_LOCAL_TASK_QUEUE
            steal_task(this->_global_tqueue, st->_local_tqueue, M_MAX_LOCAL_TASK_QUEUE);

            _schedule_base_queue_st_* to = st->_local_tqueue;
            if (_thr_cnt <= 1 || to->size() > 0) {
                // can't steal from self
                return;
            }

            // steal from other queue
            std::vector<schedule_thread_st_type> work_thrs;
            get_work_threads(work_thrs);
            if (work_thrs.empty()) {
                return;
            }

            uint64_t full = work_thrs.size();
            uint64_t rn = (uint64_t)(&work_thrs);
            rn %= full;
            rn = (uint64_t)(uintptr_t)work_thrs[(uint32_t)rn];
            rn %= full;

            while (full > 0) {
                full--;
                auto i = (rn++) % work_thrs.size();
                if (work_thrs[(uint32_t)i] == st) {
                    continue;
                }

                auto from = work_thrs[(uint32_t)i]->_local_tqueue;
                if (!from) {
                    continue;
                }

                steal_task(from, to, 0);
                if (to->size() > 0) {
                    break;
                }
            }
        }

        void _scheduler_st_::get_work_threads(std::vector<schedule_thread_st_type>& thrs) {
            std::shared_lock<std::shared_mutex> sl(this->_thrs_mu);
            thrs.clear();
            thrs.insert(thrs.end(), this->_work_threads.begin(), this->_work_threads.end());
        }

        void _scheduler_st_::get_work_idle_threads(std::vector<schedule_thread_st_type>& work,
                                                   std::vector<schedule_thread_st_type>& idle) {
            std::shared_lock<std::shared_mutex> sl(this->_thrs_mu);
            work.clear();
            idle.clear();
            work.insert(work.end(), this->_work_threads.begin(), this->_work_threads.end());
            idle.insert(idle.end(), this->_idle_threads.begin(), this->_idle_threads.end());
        }

        void _scheduler_st_::add_idle_thread(_schedule_thread_st_* st) {
            std::unique_lock<std::shared_mutex> sl(this->_thrs_mu);

            // remove from work threads
            for (auto iter = this->_work_threads.begin(); iter != this->_work_threads.end(); ++iter) {
                if (*iter == st) {
                    this->_work_threads.erase(iter);
                    break;
                }
            }

            this->_idle_threads.push_back(st);
        }

        void _scheduler_st_::remove_idle_thread(_schedule_thread_st_* st) {
            std::unique_lock<std::shared_mutex> sl(this->_thrs_mu);
            for (auto iter = this->_idle_threads.begin(); iter != this->_idle_threads.end(); ++iter) {
                if (*iter == st) {
                    this->_idle_threads.erase(iter);
                    break;
                }
            }

            // add to work threads
            this->_work_threads.push_back(st);
        }

        void _scheduler_st_::delete_idle_thread(_schedule_thread_st_* st, int64_t now) {
            std::unique_lock<std::shared_mutex> sl(this->_thrs_mu);
            for (auto iter = this->_idle_threads.begin(); iter != this->_idle_threads.end(); ++iter) {
                if (*iter == st) {
                    this->_idle_threads.erase(iter);
                    this->_dead_threads.push_back(_schedule_dead_thread_st_ {st, now});
                    break;
                }
            }
        }

        void _scheduler_st_::add_work_thread(_schedule_thread_st_* st) {
            std::unique_lock<std::shared_mutex> sl(this->_thrs_mu);
            this->_work_threads.push_back(st);
        }

        void _scheduler_st_::print_debug_info(bool enable) {
            this->_print_debug_info = enable;
        }

        void _scheduler_st_::work_func(_schedule_thread_st_* st) {
            st->on_init();

            for (;;) {
                if (st->is_stop()) {
                    break;
                }

                st->on_run();

                if (st->_scheduler->_global_tqueue->size() == 0) {
                    // 进入空闲线程队列
                    st->_scheduler->add_idle_thread(st);
                    // 将线程挂起来
                    st->wait();
                }
            }
        }

        void _scheduler_st_::watch_func() {
            auto& st = scheduler_inst();
            g_local_tqueue = st._global_tqueue;
            std::vector<schedule_thread_st_type> work_thrs;
            std::vector<schedule_thread_st_type> idle_thrs;
            size_t idle_thr_idx = 0;
            int64_t idle_beg_time = 0;
            int idles = 0;

            auto get_idle = [&idle_thrs, &idle_thr_idx]()->schedule_thread_st_type {
                if (idle_thr_idx >= idle_thrs.size()) {
                    return nullptr;
                }

                auto thr = idle_thrs[idle_thr_idx];
                idle_thr_idx++;
                return thr;
            };

            auto debug_info = [&st]() {
                auto now = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
                static int64_t beg_time = now;
                if (now - beg_time < 60) {
                    return;
                }

                beg_time = now;
                std::string output = "queue info:\n";
                output += std::string("global queue count:") + std::to_string(st._global_tqueue->size());
                output += std::string(", global task operation count:") + std::to_string(st._task_op_cnt);
                output += std::string("\n");

                std::vector<schedule_thread_st_type> work_thrs;
                std::vector<schedule_thread_st_type> idle_thrs;
                st.get_work_idle_threads(work_thrs, idle_thrs);

                for (int i = 0; i < 2; i++) {
                    std::vector<schedule_thread_st_type>* thrs = 0;
                    if (i == 0 && !work_thrs.empty()) {
                        thrs = &work_thrs;
                        output += "working threads:\n";
                    }
                    if (i == 1 && !idle_thrs.empty()) {
                        thrs = &idle_thrs;
                        output += "idle threads:\n";
                    }

                    if (!thrs) { continue; }
                    for (auto thr : *thrs) {
                        auto local_task = thr->_local_tqueue;
                        if (!local_task) {
                            continue;
                        }
                        output += std::string("work_id:") + std::to_string(thr->_work_id) + std::string(" local queue count:") + std::to_string(local_task->size());
                        output += std::string(", local task operation count:") + std::to_string(thr->_task_op_cnt);
                        output += std::string("\n");
                    }
                }

                M_CO_DEBUG_PRINT("[cgo debug] %s\n", output.c_str());
            };

            auto dispatch = [&st, &idle_thrs, &work_thrs, &idle_thr_idx](_schedule_base_queue_st_* q) {
                if (idle_thr_idx < idle_thrs.size()) {
                    // 分配空闲线程
                    auto thr = idle_thrs[idle_thr_idx];
                    idle_thr_idx++;
                    st.remove_idle_thread(thr);
                    thr->resume(q);
                } else {
                    if (work_thrs.empty() || q->secondclass_delay()) {
                        // 新起线程
                        if (!st.start_thread(q) && q->emergency()) {
                            M_CO_DEBUG_PRINT("[cgo warning!!!] all working threads were occupied for a long time(over ten seconds), maybe dead lock or being blocked\n");
                        }
                    }
                }
            };

            for (;;) {
                g_local_tqueue = st._global_tqueue;

                if (st._stop) {
                    break;
                }

                if (st._print_debug_info) {
                    debug_info();
                }

                st.run();
                st.get_work_idle_threads(work_thrs, idle_thrs);
                idle_thr_idx = 0;

                if (st._global_tqueue->size()) {
                    dispatch(st._global_tqueue);
                }

                for (auto& thr : work_thrs) {
                    if (thr->_local_tqueue->delay()) {
                        dispatch(thr->_local_tqueue);
                    }
                }

                if (idle_thrs.empty() && !work_thrs.empty()) {
                    idle_beg_time = 0;
                    idles = 0;
                } else {
                    idles++;
                    auto now = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
                    if (idle_beg_time == 0) {
                        idle_beg_time = now;
                    }
                    if (now - idle_beg_time >= M_CO_IDLE_TIME) {
                        int over = st._thr_cnt - st._core_thr_cnt;
                        for (int i = 0; i < over; i++) {
                            idle_beg_time = 0;
                            auto thr = get_idle();
                            if (!thr) {
                                break;
                            }
                            st._thr_cnt--;
                            st.delete_idle_thread(thr, now);
                        }
                    }
                }

                if (!st._dead_threads.empty()) {
                    auto now = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
                    for (auto iter = st._dead_threads.begin(); iter != st._dead_threads.end(); ) {
                        auto& dead = *iter;
                        if (now - dead._time >= 5) {
                            dead._thr->stop();
                            iter = st._dead_threads.erase(iter);
                        } else {
                            iter++;
                        }
                    }
                }

                if (idles >= 100) {
                    std::this_thread::sleep_for(std::chrono::microseconds (100));
                }
            }
        }

        void _scheduler_st_::notify_wait(integer_async_time_pool::Node* node) {
            schedule_post_signal(node->payload, (void*)0);
        }

        void set_cgo_procs(int cnt) {
            if (cnt < 1) {
                cnt = 1;
            }
            scheduler_inst()._max_thr_cnt = cnt;
        }

        void set_cgo_core(int cnt) {
            if (cnt < 1) {
                cnt = 1;
            }
            scheduler_inst()._core_thr_cnt = cnt;
        }

        void cgo_add_loop(const routine_fn& f) {
            scheduler_inst()._loops_buf.enqueue(f);
        }

        void cgo_stop() {
            M_CO_DEBUG_PRINT("[cgo debug] cgo stop\n");
            scheduler_inst().stop();
        }

        void co_hook(bool enable) {
            coro_adapter::co_hook(enable);
        }

        bool co_hook() {
            return coro_adapter::co_hook();
        }

        void print_debug_info(bool enable) {
            scheduler_inst().print_debug_info();
        }

        void cgo_default_stack(int stack) {
            if (stack <= 0) return;
            scheduler_inst()._default_stack = stack;
        }

        _scheduler_st_& scheduler_inst() {
            static _scheduler_st_ s_scheduler;
            return s_scheduler;
        }

        void add_global_task(_schedule_task_st_* task) {
            auto global = g_global_tqueue;
            auto local = g_local_tqueue;

            if (local == global) {
                local->enqueue(task);
            } else {
                if (local->size() >= M_MAX_LOCAL_TASK_QUEUE) {
                    global->enqueue(task);
                } else {
                    local->enqueue(task);
                }
            }
        }

        uint64_t cur_coid() {
            return coro_adapter::cur_coid();
        }

        // thread-safety
        void schedule_task(const routine_fn& routine, int stack, const char* file, int line) {
            auto task = new _schedule_newco_task_st_;
            task->_fn = routine;
            task->_statck = stack;
            task->_f = file;
            task->_l = line;
            task->_type = _schedule_task_st_::RunCo;
            add_global_task(task);
        }

        void schedule_yield() {
            coro_adapter::yield_co();
        }

        void schedule_wait_signal() {
            void* data;
            coro_adapter::co_wait_signal(data);
        }

        void schedule_wait_signal(void*& data) {
            coro_adapter::co_wait_signal(data);
        }

        void schedule_post_signal(uint64_t co_id, void* data) {
            assert(co_id != M_INVALID_COROUTINE_ID);
            auto task = new _schedule_comm_task_st_;
            task->_co_id = co_id;
            task->_data = data;
            task->_type = _schedule_task_st_::PostSignal;
            add_global_task(task);
        }

        // thread-safety
        void schedule_co(uint64_t co_id) {
            assert(co_id != M_INVALID_COROUTINE_ID);
            auto task = new _schedule_comm_task_st_;
            task->_co_id = co_id;
            task->_data = 0;
            task->_type = _schedule_task_st_::ResumeCo;
            add_global_task(task);
        }

        void schedule_wait(int wait_mil) {
            assert(wait_mil <= M_MAX_CO_WAIT_TIME * 1000);
            auto co_id = coro_adapter::cur_coid();
            if (co_id == M_INVALID_COROUTINE_ID) {
                return;
            }

            scheduler_inst()._time_pool.add_timer(wait_mil, co_id);
            void* data = 0;
            schedule_wait_signal(data);
        }
    }
}
