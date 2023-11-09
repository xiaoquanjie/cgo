#include <iostream>
#include <mutex>
#include <condition_variable>
#include <thread>
#include "cgo.h"

void print_withtime(const char* msg) {
    static std::mutex mu;
    std::unique_lock<std::mutex> lock(mu);
    std::cout << std::chrono::system_clock::now().time_since_epoch().count()/1000 << " " << std::this_thread::get_id() << " " << msg << "\n";
}

void cgo_test() {
    CgoProcs(10);

    bool* stop = new bool(true);
   
    Cgo[stop]() {
        while (*stop) {
            print_withtime("this is a coroutine1");
            CgoWait(1000);
        }
        print_withtime("this is a coroutine1 over");
    };

    Cgo [stop]() {
        while (*stop) {
            print_withtime("this is a coroutine2");
            CgoWait(50);
        }
        print_withtime("this is a coroutine2 over");
    };

    Cgo[stop]() {
        while (*stop) {
            print_withtime("this is a coroutine3");
            CgoWait(50);
        }
        print_withtime("this is a coroutine3 over");
    };

    Cgo[stop]() {
        for (int i = 0; i < 100; i++) {
            CgoWait(5);
        }
        *stop = false;
    };
}

void cond_test() {
    std::mutex mu;
    std::condition_variable cond;
    cond.notify_one();

    std::thread thr1([&]() {
        while (true) {
            std::unique_lock<std::mutex> lock(mu);
            cond.wait_for(lock, std::chrono::seconds(20));
            print_withtime("hello1");
        }
    });
    thr1.detach();

    std::thread thr2([&]() {
        while (true) {
            std::unique_lock<std::mutex> lock(mu);
            cond.wait_for(lock, std::chrono::seconds(20));
            print_withtime("hello2");
        }
    });
    thr2.detach();

    while (true) {
        int i = 0;
        std::cin >> i;
        cond.notify_one();
    }
}

int main()
{
    //cond_test();
    cgo_test();
    std::cout << "Hello World!\n";
    int i = 0;
    std::cin >> i;
}
