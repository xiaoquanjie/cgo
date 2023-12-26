#include <atomic>
#include <thread>
#include <iostream>
#include <assert.h>
#include <mutex>
#include <memory>
#include <string>

#include "../cgo/coroutine/coroutine_adapter.h"

void print_withtime(const std::string& msg);
void print_withtime(const char* msg);

#define NUM_TASKS 16
uint64_t tasks[NUM_TASKS];
std::atomic_flag flags[NUM_TASKS];

void fun1() { // Ïß³Ì1
    static std::mutex mu;
    while (true) {
        for (int i = 0; i < NUM_TASKS; i++) {
            //if (cgo::coro_adapter::)
            if (!flags[i].test_and_set()) {
                cgo::coro_adapter::resume_co(tasks[i], (void*)1);
                flags[i].clear();
            }
        }
    }
}

void rouinte() {
    int count = 0;
    while (true) {
        count++;
        auto co_id = cgo::coro_adapter::cur_coid();
        print_withtime("co output:" + std::to_string((uintptr_t)co_id) + " count:" + std::to_string(count));
        cgo::coro_adapter::yield_co();
    }
}

void hook_test() {

}

int main2() {
    //hook();
    hook_test();
    return 0;

}
