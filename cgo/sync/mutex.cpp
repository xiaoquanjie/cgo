//
// Created by xiaoqj on 2024/1/15.
//

#include "mutex.h"
#include "common/concurrentqueue.h"
#include "scheduler/scheduler.h"

#if defined(__GNUC__)
#include <semaphore.h>
#else
#include <winbase.h>
typedef void* sem_t;
#endif

#define M_TASK_QUEUE(que) \
((moodycamel::ConcurrentQueue<task_struct>*)que)

#define M_INIT_QUEUE() \
(void*)(new moodycamel::ConcurrentQueue<task_struct>)

#define M_RELEASE_QUEUE(que) \
delete M_TASK_QUEUE(que)

#define M_IN_CO() \
(scheduler::cur_coid() != (uint64_t)-1)

#define M_OWNER(own) \
((task_id*)own)

#define M_INIT_OWNER(own) \
((task_id*)own)->id = 0; ((task_id*)own)->is_co = false;

namespace cgo {
    struct task_id {
        unsigned long long id;
        bool is_co;
    };

    struct task_sem {
        void* sem;
    };

    struct task_struct {
        task_id tid;
        void* sem;
    };

    inline task_id get_self() {
        if (M_IN_CO()) {
            return task_id {
                    scheduler::cur_coid(),
                    true
            };
        }
#ifdef __GNUC__
        auto tid = pthread_self();
#else
        auto tid = GetCurrentThreadId();
#endif
        return task_id {
                (unsigned long long)tid,
                false
        };
    }

    inline bool task_id_equal(task_id* id1, task_id* id2) {
        return (id1->is_co == id2->is_co) && (id1->id == id2->id);
    }

    inline task_struct get_task() {
        if (M_IN_CO()) {
            return task_struct {
                    get_self(),
                    0
            };
        }

#ifdef __GNUC__
        sem_t* sem = (sem_t*)malloc(sizeof(sem_t));
        sem_init(sem, 0, 0);
#else
        auto sem = CreateSemaphore(NULL, 1, 0, NULL);
#endif
        return task_struct {
            get_self(),
            (void*)sem
        };
    }

    inline void release_task(task_struct* task) {
        if (task->tid.is_co) {
            return;
        }
#ifdef __GNUC__
        sem_t* sem = (sem_t*)task->sem;
        sem_destroy(sem);
        free(sem);
#else
        CloseHandle((HANDLE)task->sem);
#endif
    }

    inline void wait_task(task_struct* task) {
        if (task->tid.is_co) {
            scheduler::schedule_yield();
            return;
        }
#ifdef __GNUC__
        sem_wait((sem_t*)task->sem);
#else
        WaitForSingleObject((HANDLE)task->sem, INFINITE);
#endif
    }

    inline void resume_task(task_struct* task) {
        if (task->tid.is_co) {
            scheduler::schedule_co(task->tid.id, 0);
            return;
        }
#ifdef __GNUC__
        sem_post((sem_t*)task->sem);
#else
        ReleaseSemaphore((HANDLE)task->sem, 1, NULL);
#endif
    }

    co_mutex::co_mutex() {
        _task_queue = M_INIT_QUEUE();
        _lock = false;
        _owner = malloc(sizeof(task_id));
        M_INIT_OWNER(_owner);
    }

    co_mutex::~co_mutex() {
        M_RELEASE_QUEUE(_task_queue);
        free(_owner);
    }

    void co_mutex::lock() {
        if (try_lock()) {
            return;
        }

        auto self = get_self();
        if (task_id_equal(&self, M_OWNER(_owner))) {
            throw "not recursive lock";
        }

        auto task = get_task();
        M_TASK_QUEUE(_task_queue)->enqueue(task);
        wait_task(&task);
        release_task(&task);
    }

    // 不允许重入
    bool co_mutex::try_lock() {
        bool old = false;
        if (_lock.compare_exchange_weak(old, true, std::memory_order_relaxed)) {
            *M_OWNER(_owner) = get_self();
            return true;
        }
        return false;
    }

    void co_mutex::unlock() {
        auto self = get_self();
        if (!task_id_equal(&self, M_OWNER(_owner))) {
            throw "not lock owner";
        }

        task_struct task;
        if (M_TASK_QUEUE(_task_queue)->try_dequeue(task)) {
            // 更换所有者
            *M_OWNER(_owner) = task.tid;
            resume_task(&task);
        } else {
            M_INIT_OWNER(_owner);
            bool old = true;
            _lock.compare_exchange_weak(old, false, std::memory_order_relaxed);
        }
    }
}