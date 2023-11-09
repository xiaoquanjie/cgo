#include <iostream>
#include <mutex>
#include <thread>
#include "cgo.h"

void print_withtime(const char* msg) {
    static std::mutex mu;
    std::unique_lock<std::mutex> lock(mu);
    std::cout << std::chrono::system_clock::now().time_since_epoch().count()/1000 << " " << std::this_thread::get_id() << " " << msg << "\n";
}

void cgo_test() {
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

int main()
{
    cgo_test();
    std::cout << "Hello World!\n";
    int i = 0;
    std::cin >> i;
}
