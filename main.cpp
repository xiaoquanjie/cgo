#include <iostream>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <string>
#include "cgo.h"

void pause() {
	while (true) {
		std::this_thread::sleep_for(std::chrono::seconds(10));
		break;
	}
	gostop();
}

void print_withtime(const char* msg) {
    static std::mutex mu;
    std::unique_lock<std::mutex> lock(mu);
    std::cout << std::chrono::system_clock::now().time_since_epoch().count()/1000 << " " << std::this_thread::get_id() << " " << msg << "\n";
}

void cgo_test() {
    bool* stop = new bool(true);
    goprocs(100);
   
    go [stop]() {
        while (*stop) {
            print_withtime("this is a coroutine1");
            gowait(1000);
        }
        print_withtime("this is a coroutine1 over");
    };

    go [stop]() {
        while (*stop) {
            print_withtime("this is a coroutine2");
            gowait(50);
        }
        print_withtime("this is a coroutine2 over");
    };

    go [stop]() {
        while (*stop) {
            print_withtime("this is a coroutine3");
            gowait(50);
        }
        print_withtime("this is a coroutine3 over");
    };

    go[stop]() {
        for (int i = 0; i < 100; i++) {
            gowait(5);
        }
        *stop = false;

        go[]() {
            print_withtime("create in sub coroutine");
        };
    };

    go gostack(1024*8) []() {
        print_withtime("self stack 8192");
    };

    /*Cgo {
        1024,
        []() {

        }
    };*/

    while (true) {
        int input = 0;
        std::cout << "input count:\n";
        std::cin >> input;
        for (int i = 0; i < input; i++) {
            go[i]() {
                print_withtime((std::string("coroutine no:") + std::to_string(i)).c_str());
            };
        }
    }
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

#include "common/time_pool.h"
void time_test() {
    async_time_pool p;
    std::thread([&p]() {
        for (int i = 0; i < 10; i++) {
            p.async_add_timer(30, [i]() {
                print_withtime(std::to_string(i).c_str());
            });
        }
    }).detach();

    while (true) {
        p.update();
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
}

void lock_test() {
    // deadlock test

    goprocs(1);
    std::mutex mu;
    go [&mu]() {
        while (true) {
            std::unique_lock<std::mutex> lock(mu);
            print_withtime("lock1");
            gowait(2000);
        }
    };

    go [&mu]() {
        while (true) {
            std::unique_lock<std::mutex> lock(mu);
            print_withtime("lock2");
            gowait(2000);
        }
    };

    pause();
}

#include "common/circle_queue.h"
void cqueue_test() {
    struct info {
        int i = 0;
    };

    cqueue<info> q(3);
    q.push(info{10});
    q.push(info{20});
    q.push(info{30});
    q.push(info{40});
    info i2;
    q.pop(i2);
    q.push(info{50});
    q.pop(i2);
    q.pop(i2);
    q.push(info{60});
    q.push(info{70});
    q.push(info{80});
    std::cout << q.empty() << "\n";
    std::cout << q.full() << "\n";
    while (q.size()) {
        q.pop(i2);
        std::cout << i2.i << "\n";
    }
}

void chan_test() {
    cgo::chan<int> ch;
    ch = makechan<int>(0);
	std::cout << "ref:" << ch.use_count() << "\n";
	
	go [ch]() {
        while (true) {
            int i;
            auto ok = ch << i;
			if (!ok) {
				break;
			}
            print_withtime(("co1:" + std::to_string(i)).c_str());
        }
    };

	go[ch]() {
		while (true) {
			int i;
			auto ok = ch << i;
			if (!ok) {
				break;
			}
			print_withtime(("co2:" + std::to_string(i)).c_str());
		}
	};

	go[ch]() {
		std::cout << "ref:" << ch.use_count() << "\n";
		
		for (int i = 0; i < 100; i++) {
			ch >> i;
		}

		//closechan(ch);
	};
}

int main()
{
    chan_test();
    //cqueue_test();
    //lock_test();
    //time_test();
    //cond_test();
    //cgo_test();
    pause();
	std::cout << "finish\n";
	return 0;
}
