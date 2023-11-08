#include <iostream>
#include <thread>
#include <mutex>
#include "cgo.h"
#include "common/slist.h"
#include "common/time_pool.h"

std::string get_date_time()
{
    auto to_string = [](const std::chrono::system_clock::time_point& t)->std::string
    {
        auto as_time_t = std::chrono::system_clock::to_time_t(t);
        struct tm tm;
#if defined(WIN32) || defined(_WINDLL)
        localtime_s(&tm, &as_time_t);  //win api，线程安全，而std::localtime线程不安全
#else
        localtime_r(&as_time_t, &tm);//linux api，线程安全
#endif

        std::chrono::milliseconds ms = std::chrono::duration_cast<std::chrono::milliseconds>(t.time_since_epoch());
        char buf[128];
        snprintf(buf, sizeof(buf), "%04d-%02d-%02d %02d:%02d:%02d %03lld ",
                 tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec, ms.count() % 1000);
        return buf;
    };

    std::chrono::system_clock::time_point t = std::chrono::system_clock::now();
    return to_string(t);
}

void print_withtime(const char* msg) {
    static std::mutex mu;
    std::unique_lock<std::mutex> lock(mu);
    std::cout << get_date_time() << " " << std::this_thread::get_id() << " " << msg << "\n";
}

//void main_thread_test() {
//    std::vector<int64_t> ids;
//
//    cgo::coroutine::run([&ids]() {
//        int i = 10;
//        ids.push_back(cgo::coroutine::curid());
//        std::cout << cgo::coroutine::curid() << " " << i++ << std::endl;
//        cgo::coroutine::yield();
//        std::cout << i++ << std::endl;
//    });
//
//    std::cout << "main\n";
//
//    cgo::coroutine::run([&ids]() {
//        int i = 101;
//        ids.push_back(cgo::coroutine::curid());
//        std::cout << cgo::coroutine::curid() << " " << i++ << std::endl;
//        cgo::coroutine::yield();
//        std::cout << i++ << std::endl;
//    });
//
//    std::cout << cgo::coroutine::num() << " " << cgo::coroutine::memory() << "\n";
//
//    for (auto id : ids) {
//        cgo::coroutine::resume(id);
//    }
//
//    std::cout << cgo::coroutine::num() << " " << cgo::coroutine::memory() << "\n";
//}

//void sub_thread_test() {
//    auto id = cgo::coroutine::create([]() {
//        int i = 0;
//        std::cout << "hello" << i++ << std::endl;
//        cgo::coroutine::yield();
//        std::cout << "hello" << i++ << std::endl;
//    });
//
//    std::thread thr([id]() {
//        cgo::coroutine::resume(id);
//        std::cout << "sub thread\n";
//        cgo::coroutine::resume(id);
//    });
//    thr.join();
//}

void slist_test() {
    slist<int> s;
    s.iterate([](int& v)->bool{
       return v != 3;
    });

    s.push(1);
    //s.push(2);
    s.push(3);
    s.push(4);
    s.push(3);
    s.push(4);

    s.iterate([](int& v)->bool{
        return v != 1;
    });

    s.iterate([](int& v)->bool{
        return v != 3;
    });

    s.iterate([](int& v)->bool{
        return v != 4;
    });
}

void cgo_test() {
    bool* stop = new bool(false);
    Cgo [stop]() {
        std::string tmp = "this is a man from china:";
        int i = 0;
        while (*stop == false) {
            auto msg = tmp + std::to_string(i++);
            print_withtime(msg.c_str());
            CgoWait(200);
        }
    };

    Cgo [stop]() {
        std::string tmp = "this is a man from hongkong:";
        int i = 0;
        while (*stop == false) {
            auto msg = tmp + std::to_string(i++);
            print_withtime(msg.c_str());
            CgoWait(200);
        }
    };

    Cgo [stop]() {
        for (int i = 0; i < 20; i++) {
            CgoWait(200);
        }
        *stop = true;
    };

}

void timepool_test() {
    print_withtime("start\n");

    async_time_pool p;

    p.async_add_timer(1000, []() {
        print_withtime("after 1000ms\n");
    });

    p.async_add_timer(50, []() {
        print_withtime("after 50ms\n");
    });

    auto id = p.async_add_timer(60, []() {
        print_withtime("after 60ms\n");
    });

    p.async_cancel_timer(id);
    //p.cancel_timer(id);

    p.async_add_timer(5000, []() {
        print_withtime("after 5000ms\n");
    });

    p.async_add_timer(1000*60, []() {
        print_withtime("after 1min\n");
    });

    p.async_add_timer(1000*60*10, []() {
        print_withtime("after 10min\n");
    });

    while (true) {
        p.update();
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
}

void pause() {
    int i = 0;
    print_withtime("main thread pause");
    std::cin >> i;
}


int main() {

    //sub_thread_test();
    //slist_test();
    cgo_test();
    //timepool_test();

    pause();
    return 0;
}
