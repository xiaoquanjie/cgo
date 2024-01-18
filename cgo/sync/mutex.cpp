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
#define pthread_self GetCurrentThreadId
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
((task_id*)own)->id = 0;

#define M_GET_SELF(id) id = scheduler::cur_coid();

namespace cgo {
    struct task_id {
        unsigned long long id;
    };

    struct task_sem {
        void* sem;
    };

    struct task_struct {
        task_id tid;
        void* sem;
    };

    inline void get_task(task_struct* t) {
        if (M_IN_CO()) {
            t->sem = 0;
            M_GET_SELF(t->tid.id);
            return;
        }

#ifdef __GNUC__
        sem_t* sem = (sem_t*)malloc(sizeof(sem_t));
        sem_init(sem, 0, 0);
#else
        auto sem = CreateSemaphore(NULL, 1, 0, NULL);
#endif
        t->sem = (void*)sem;
        M_GET_SELF(t->tid.id);
        return;
    }

    inline void release_task(task_struct* task) {
        if (task->tid.id != (unsigned long long)-1) {
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
        if (task->tid.id != (unsigned long long)-1) {
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
        if (task->tid.id != (unsigned long long)-1) {
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
        _lock.clear();
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

        task_id self;
        M_GET_SELF(self.id);
        if (self.id != M_OWNER(_owner)->id) {
            throw "not recursive lock";
        }

        task_struct task;
        get_task(&task);
        M_TASK_QUEUE(_task_queue)->enqueue(task);
        wait_task(&task);
        release_task(&task);
    }

    // 不允许重入
    bool co_mutex::try_lock() {
        if (!_lock.test_and_set()) {
            M_GET_SELF(M_OWNER(_owner)->id);
            return true;
        }
        return false;
    }

    void co_mutex::unlock() {
        task_id self;
        M_GET_SELF(self.id);
        if (self.id != M_OWNER(_owner)->id) {
            throw "not lock owner";
        }

        task_struct task;
        if (M_TASK_QUEUE(_task_queue)->try_dequeue(task)) {
            // 更换所有者
            *M_OWNER(_owner) = task.tid;
            resume_task(&task);
        } else {
            M_INIT_OWNER(_owner);
            _lock.clear();
        }
    }
}