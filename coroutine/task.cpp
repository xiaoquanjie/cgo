//
// Created by rong on 2023/11/4.
//

#include "task.h"

namespace cgo {
    namespace coroutine {
        void _wait_task_list_st_::push(wait_task_ptr task) {
            std::unique_lock<std::mutex> lock(_mu);
            _list.push(task);
            _cond.notify_one();
        }

        void _wait_task_list_st_::push(const char* file, int line, std::function<void()> routine) {
            auto task = std::make_shared<_wait_task_st_>();
            task->_file = file;
            task->_line = line;
            task->_routine = routine;
            push(task);
        }

        wait_task_ptr _wait_task_list_st_::pop(int wait_time) {
            auto duration = std::chrono::microseconds(wait_time);
            std::unique_lock<std::mutex> lock(_mu);
            if (_list.empty()) {
                _cond.wait_for(lock, duration);
            }

            if (_list.empty()) {
                return nullptr;
            } else {
                auto task = _list.front();
                _list.pop();
                return task;
            }
        }

        void _co_task_list_st_::push(co_task_ptr task) {
            std::unique_lock<std::mutex> lock(_mu);
            _list.push(task);
            _cond.notify_one();
        }

        void _co_task_list_st_::push(int64_t co_id) {
            auto task = std::make_shared<_co_task_st_>();
            task->_co_id = co_id;
            push(task);
        }

        co_task_ptr _co_task_list_st_::pop(int wait_time) {
            auto duration = std::chrono::microseconds(wait_time);
            std::unique_lock<std::mutex> lock(_mu);
            if (_list.empty()) {
                _cond.wait_for(lock, duration);
            }

            if (_list.empty()) {
                return nullptr;
            } else {
                auto task = _list.front();
                _list.pop();
                return task;
            }
        }

        _wait_task_list_st_ gwaittask;

#ifdef M_PLATFORM_WIN
        thread_local _co_task_list_st_ gcotask;
#else
        _co_task_list_st_ gcotask;
#endif
    }
}