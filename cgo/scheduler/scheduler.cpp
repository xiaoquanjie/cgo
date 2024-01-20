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
        void resume_co(uint64_t co_id, void* data);
        void yield_co(void*& data, const std::function<void()>& after);
        void yield_co(void*& data);
        void yield_co();
        void run_co(const std::function<void()>& routine, int stack, const char* file, int line);
        uint64_t cur_coid();
        void co_hook(bool enable);
        bool co_hook();
    }

    namespace scheduler {
        using concurrent_task_queue_type = moodycamel::ConcurrentQueue<task_type>;
        using wsq_task_queue_type = WorkStealingQueue<task_type*>;

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
            virtual void enqueue(const task_type& f) = 0;
            virtual void enqueue(task_type* f, bool&) = 0;
            virtual bool try_dequeue(task_type& f) = 0;
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
            inline void enqueue(const task_type& f) override;
            inline void enqueue(task_type* f, bool&) override;
            bool try_dequeue(task_type& f) override;
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
            inline void enqueue(const task_type& f) override;
            inline void enqueue(task_type* f, bool&) override;
            bool try_dequeue(task_type& f) override;
            void steal(int32_t count, _schedule_base_queue_st_* to) override;
        };

        struct _schedule_thread_st_ {
            int _work_id = 0;
            std::thread* _thr = 0;
            _scheduler_st_* _scheduler = 0;
            _schedule_base_queue_st_* _local_task = 0;
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
            _schedule_global_queue_st_* _global_tasks;
            std::vector<schedule_thread_st_type> _work_threads; // 正在工作的线程
            std::vector<schedule_thread_st_type> _idle_threads; // 空闲的线程
            std::vector<_schedule_dead_thread_st_> _dead_threads;
            int generate_work_id = 1;
            std::shared_mutex _thrs_mu;
            _schedule_watch_thread_st_ _watcher;

            std::atomic_bool _stop = false;
            std::atomic_int _max_thr_cnt = 0;   // max thread count
            std::atomic_int _core_thr_cnt = 1;  // core thread count
            std::atomic_int _thr_cnt = 0;       // current thread count
            std::atomic_uint64_t _task_op_cnt = 0;

            async_time_pool _time_pool;
            std::vector<task_type> _loops;
            concurrent_task_queue_type _loops_buf;
            bool _print_debug_info = false;

            _scheduler_st_();

            ~_scheduler_st_();

            void stop();

            bool run();

            bool can_start();

            bool start_thread(_schedule_base_queue_st_* newq);

            void steal_task(_schedule_base_queue_st_* from, _schedule_base_queue_st_* to, int32_t count);

            void steal_task(_schedule_thread_st_* st);

            void get_work_threads(std::vector<schedule_thread_st_type>& thrs);

            void get_idle_threads(std::vector<schedule_thread_st_type>& thrs);

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

        inline void _schedule_local_queue_st_::enqueue(const std::function<void()>& f) {
            task_type* task = new task_type(f);
            // Only the owner thread can insert an item to the queue.
            _queue->push(task);
            record();
        }

        inline void _schedule_local_queue_st_::enqueue(task_type* f, bool& forward) {
            _queue->push(f);
            forward = true;
            record();
        }

        bool _schedule_local_queue_st_::try_dequeue(std::function<void()>& f) {
            auto opt = _queue->pop();
            if (opt == std::nullopt) {
                return false;
            }

            f = *opt.value();
            delete opt.value();
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

                bool forward = false;
                to->enqueue(opt.value(), forward);
                if (!forward) {
                    delete opt.value();
                }
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

        inline void _schedule_global_queue_st_::enqueue(const task_type& f) {
            // 这也是一个瓶颈
            assert(_queue->enqueue(f));
            record();
        }

        inline void _schedule_global_queue_st_::enqueue(task_type* f, bool& forward) {
            // 这也是一个瓶颈
            assert(_queue->enqueue(*f));
            forward = false;
            record();
        }

        bool _schedule_global_queue_st_::try_dequeue(task_type& f) {
            auto ret = _queue->try_dequeue(f);
            if (ret) {
                unrecord();
            }
            return ret;
        }

        void _schedule_global_queue_st_::steal(int32_t count, _schedule_base_queue_st_* to) {
            while (count > 0) {
                count--;
                task_type f;
                if (!_queue->try_dequeue(f)) {
                    break;
                }
                to->enqueue(f);
            }
            unrecord();
        }

        //////////////////////////////////////////////////////////////

        _schedule_base_queue_st_* gglobal_task_queue = scheduler_inst()._global_tasks;
        thread_local _schedule_base_queue_st_* volatile glocal_task_queue = gglobal_task_queue;

        //////////////////////////////////////////////////////////////

        _schedule_thread_st_* _schedule_thread_st_::create(_scheduler_st_* s,
                                                           int wid,
                                                           _schedule_base_queue_st_* fromq) {
            auto st = new _schedule_thread_st_;
            st->_work_id = wid;
            st->_local_task = new _schedule_local_queue_st_;
            st->_scheduler = s;

            if (fromq) {
                s->steal_task(fromq, st->_local_task, 0);
            }

            return st;
        }

        void _schedule_thread_st_::on_init() {
            M_CO_DEBUG_PRINT("[cgo debug] start working thread:%d\n", this->_work_id);
            glocal_task_queue = this->_local_task;
        }

        void _schedule_thread_st_::on_release() {
            M_CO_DEBUG_PRINT("[cgo debug] quit working thread:%d\n", this->_work_id);

            glocal_task_queue = nullptr;

            if (this->_local_task) {
                if (!_scheduler->_stop) {
                    if (this->_local_task->size() > 0) {
                        assert(false);
                    }
                }
                delete this->_local_task;
                this->_local_task = nullptr;
            }

            if (this->_thr) {
                delete _thr;
                _thr = nullptr;
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
                task_type task;
                while (this->_local_task->try_dequeue(task)) {
                    task_op_cnt++;
                    task();
                    if (this->is_stop()) {
                        break;
                    }
                }

                if (this->_local_task->size() == 0) {
                    // steal from the other queue
                    this->_scheduler->steal_task(this);
                    if (this->_local_task->size() == 0) {
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
                this->_scheduler->steal_task(from, this->_local_task, 0);
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

        _scheduler_st_::_scheduler_st_() : _watcher(&_scheduler_st_::watch_func) {
            _max_thr_cnt = (int)(std::thread::hardware_concurrency() * M_MAX_PROCS_FACTOR);
            _core_thr_cnt = (int)(_max_thr_cnt * M_CORE_POOL_FACTOR);
            _global_tasks = new _schedule_global_queue_st_;
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
            task_type f;
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
            if (from != gglobal_task_queue) {
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
            steal_task(this->_global_tasks, st->_local_task, M_MAX_LOCAL_TASK_QUEUE);

            _schedule_base_queue_st_* to = st->_local_task;
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

                auto from = work_thrs[(uint32_t)i]->_local_task;
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
            thrs = this->_work_threads;
        }

        void _scheduler_st_::get_idle_threads(std::vector<schedule_thread_st_type>& thrs) {
            std::shared_lock<std::shared_mutex> sl(this->_thrs_mu);
            thrs = this->_idle_threads;
        }

        void _scheduler_st_::get_work_idle_threads(std::vector<schedule_thread_st_type>& work,
                                                   std::vector<schedule_thread_st_type>& idle) {
            std::shared_lock<std::shared_mutex> sl(this->_thrs_mu);
            work = this->_work_threads;
            idle = this->_idle_threads;
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

                if (st->_scheduler->_global_tasks->size() == 0) {
                    // 进入空闲线程队列
                    st->_scheduler->add_idle_thread(st);
                    // 将线程挂起来
                    st->wait();
                }
            }
        }

        void _scheduler_st_::watch_func() {
            auto& st = scheduler_inst();
            std::vector<schedule_thread_st_type> work_thrs;
            std::vector<schedule_thread_st_type> idle_thrs;
            int64_t idle_beg_time = 0;
            int idles = 0;

            auto get_idle = [&idle_thrs]()->schedule_thread_st_type {
                if (idle_thrs.empty()) {
                    return nullptr;
                }
                auto thr = idle_thrs.back();
                idle_thrs.pop_back();
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
                output += std::string("global queue count:") + std::to_string(st._global_tasks->size());
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
                        auto local_task = thr->_local_task;
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

            auto dispatch = [&st, &idle_thrs, &work_thrs](_schedule_base_queue_st_* q) {
                if (!idle_thrs.empty()) {
                    // 分配空闲线程
                    auto thr = idle_thrs.back();
                    idle_thrs.pop_back();
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
                if (st._stop) {
                    break;
                }

                if (st._print_debug_info) {
                    debug_info();
                }

                st.run();
                st.get_work_idle_threads(work_thrs, idle_thrs);

                if (st._global_tasks->size()) {
                    dispatch(st._global_tasks);
                }

                for (auto& thr : work_thrs) {
                    if (thr->_local_task->delay()) {
                        dispatch(thr->_local_task);
                    }
                }

                if (idle_thrs.empty()) {
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

        void cgo_add_loop(const task_type& f) {
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

        _scheduler_st_& scheduler_inst() {
            static _scheduler_st_ s_scheduler;
            return s_scheduler;
        }

        void add_global_task(task_type&& f) {
            auto global = gglobal_task_queue;
            auto local = glocal_task_queue;

            if (local == global) {
                local->enqueue(f);
            } else {
                if (local->size() >= M_MAX_LOCAL_TASK_QUEUE) {
                    global->enqueue(f);
                } else {
                    local->enqueue(f);
                }
            }
        }

        uint64_t cur_coid() {
            return coro_adapter::cur_coid();
        }

        // thread-safety
        void schedule_task(const task_type& routine, int stack, const char* file, int line) {
            add_global_task([routine, stack, file, line]() {
                coro_adapter::run_co(routine, stack, file, line);
            });
        }

        void schedule_yield(void*& data, const task_type& after) {
            coro_adapter::yield_co(data, after);
        }

        void schedule_yield(void*& data) {
            coro_adapter::yield_co(data);
        }

        void schedule_yield() {
            coro_adapter::yield_co();
        }

        // thread-safety
        void schedule_co(uint64_t co_id, void* data) {
            add_global_task([co_id, data]() {
                coro_adapter::resume_co(co_id, data);
            });
        }

        void yield_after(uint64_t co_id, int wait_mil) {
            scheduler_inst()._time_pool.async_add_timer(wait_mil, [co_id] {
                add_global_task([co_id] {
                    coro_adapter::resume_co(co_id, (void*)0);
                });
            });
        }

        void schedule_wait(int wait_mil) {
            assert(wait_mil <= M_MAX_CO_WAIT_TIME * 1000);
            auto co_id = coro_adapter::cur_coid();
            if (co_id == M_INVALID_COROUTINE_ID) {
                return;
            }

            void* data = 0;
            schedule_yield(data, [co_id, wait_mil] {
                yield_after(co_id, wait_mil);
            });
        }
    }
}
