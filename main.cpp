#include <iostream>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <string>
#include <vector>
#include <iomanip>
#include "cgo.h"

void print_withtime(const char* msg) {
    static std::mutex mu;
    std::unique_lock<std::mutex> lock(mu);

    std::chrono::system_clock::time_point now = std::chrono::system_clock::now();
    std::time_t now_c = std::chrono::system_clock::to_time_t(now);
    std::tm* timeinfo = std::localtime(&now_c);

    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;

    char buffer[80];
    std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", timeinfo);

    std::cout << buffer << '.' << std::setfill('0') << std::setw(3) << ms.count() << " ";
    std::cout << std::this_thread::get_id() << " " << msg << "\n";
}

void print_withtime(const std::string& msg) {
    print_withtime(msg.c_str());
}

void pause() {
    while (true) {
        std::this_thread::sleep_for(std::chrono::seconds(500));
        break;
    }

    print_withtime("try to stop");
}

void cgo_test() {
    //cgoprocs(1);
    bool* stop = new bool(true);

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
        for (int i = 0; i < 1000; i++) {
            gowait(10);
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

void cgo_wait_test() {
    go []() {
        for (int i = 0; i < 10; i++) {
            go [i]() {
                for (int j = 0; j < 10; ++j) {
                    print_withtime(std::string("co wait") + std::to_string(i));
                    gowait(1000 + i * 10);
                }
            };
        }
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

    cgoprocs(1);
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
    /* 1000000次数据，16个协程
     * golang 测试：
     *  无缓存：300ms
     *  100缓存：100ms
     * */
    cgo::chan<int> ch;
    ch = makechan<int>(0);
	std::cout << "ref:" << ch.use_count() << "\n";
    print_withtime("begin");

    int total = 1000000;
    std::atomic_int count = 0;

	go [ch, &count, total]() {
        for (int i = 0; i < total; i++) {
            ch >> i;
        }
    };

    for (int i=0; i< 16; i++) {
        go [ch, &count]() {
            while (true) {
                int v;
                auto ok = ch << v;
                if (!ok) {
                    break;
                } else {
                    count++;
                }
            }

        };
    }

    while (count != total) {}
    print_withtime("end");
}

void performance_test() {
    cgoprocs(1);
    for (int j = 0; j < 2; j++) {
        std::atomic_int count = 0;
        const int total_count = 1;

        auto beg = std::chrono::steady_clock::now();
        for (int i = 0; i < total_count; i++) {
            go [&count] {
                count++;
            };
        }

        while (count != total_count) {
        }

        auto end = std::chrono::steady_clock::now();
        std::cout << (std::chrono::duration_cast<std::chrono::nanoseconds>(end - beg)).count() << "\n";
        std::cout << "==============\n";
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}

void test(std::atomic_int& c) {
    c++;
}

#include "coroutine/coroutine.h"
void performance_base_test() {
    for (int j = 0; j < 3; j++) {
        print_withtime("base test begin");
        std::atomic_int count = 0;
        auto beg = std::chrono::steady_clock::now();
        const int total_count = 1000000;

        for (int i = 0; i < total_count; i++) {
            cgo::coroutine::run([&count]() {
                count++;
            });
        }

        while (count != total_count) {
        }

        print_withtime("base test end");
        std::cout << "==============\n";
    }
}

#include "coroutine/scheduler.h"
void performance_copool_base_test() {
    for (int j = 0; j < 3; j++) {
        print_withtime("co pool base test begin");
        auto beg = std::chrono::steady_clock::now();
        std::atomic_int count = 0;
        const int total_count = 1000000;

        cgo::scheduler::_co_pool_st_ co_pool;
        for (int i = 0; i < total_count; i++) {
            co_pool.run([&count]() {
                count++;
            }, 0, 0);
        }

        while (count != total_count) {
        }

        auto end = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end - beg);
        print_withtime("co pool base test end: " + std::to_string(elapsed.count()));
        std::cout << "==============\n";
    }
}

void performance_test2() {
    /*
     * 基准测试结果：
     *  800ms
     * 协程池基准测试结果：
     *  700ms
     * golang测试结果:
     *  200ms
     * 测试结果：
     *  优化前：
     *      自由线程：需要30~35秒
     *      单线程：2秒
     *  优化后：
     *      自由线程: 600ms
     * */

    //cgoprocs(3);
    for (int j = 0; j < 3; j++) {
        print_withtime("begin");
        std::atomic_int count = 0;
        auto beg = std::chrono::steady_clock::now();
        const int total_count = 1000000;

        for (int i = 0; i < total_count; i++) {
            go [&count]() {
                count++;
            };
        }

        while (count != total_count) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            //cgo::cgo_print_debug_info();
            //std::cout << count << "\n";
        }

        auto end = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end - beg);
        print_withtime(std::string("end: ") + std::to_string(elapsed.count()));
        std::cout << "==============\n";
    }
}

void performance_test3() {
    //cgoprocs(2);
    go []() {
        print_withtime("start");

        for (int j = 0; j < 3; j++) {
            print_withtime("begin");
            std::atomic_int count = 0;
            auto beg = std::chrono::steady_clock::now();
            const int total_count = 1000000;

            for (int i = 0; i < total_count; i++) {
                go [&count]() {
                    count++;
                };
            }

            print_withtime("post over");
            cgo::cgo_print_debug_info();

            while (count != total_count) {
                std::this_thread::sleep_for(std::chrono::milliseconds(2000));
                cgo::cgo_print_debug_info();
            }

            auto end = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end - beg);
            print_withtime(std::string("end: ") + std::to_string(elapsed.count()));
            std::cout << "==============\n";
        }
    };
}

void atomic_test() {
    struct test_s {
        volatile int i = 0;
        std::string j;
    };

    struct test_s2 {
        int i = 0;
        std::string j;
    };

    void* m = malloc(sizeof(test_s));
    auto p = new(m)test_s;
    p->i= 2132;
    p->j = "fdfsfsdfs";
    print_withtime(p->j.c_str());
    p->~test_s();
    std::cout << sizeof(test_s) << std::endl;
    std::cout << sizeof(test_s2) << std::endl;

    std::this_thread::yield();
}

#include "common/concurrentqueue.h"
void concurrentqueue_test() {
    moodycamel::ConcurrentQueue<int> q;
    std::vector<std::thread> thrs;
    std::atomic_int produce_count = 0;
    std::atomic_int real_produce_count = 0;
    std::atomic_int consume_count = 0;
    std::atomic_int total_count = 1000000 * 3;

    auto producer = [&thrs, &q, &produce_count, &total_count, &real_produce_count]() {
        thrs.emplace_back(std::thread([&q, &produce_count, &total_count, &real_produce_count]() {
            while (produce_count.fetch_add(1) < total_count) {
                q.enqueue(produce_count);
                real_produce_count++;
            }
        }));
    };

    auto consumer = [&thrs, &q, &consume_count, &total_count]() {
        thrs.emplace_back(std::thread([&q, &consume_count, &total_count]() {
            while (true) {
                if (consume_count == total_count) {
                    break;
                }

                int v;
                if (q.try_dequeue(v)) {
                    consume_count++;
                }
            }
        }));
    };

    auto now = std::chrono::steady_clock::now();
    producer();
    producer();
    producer();
    producer();
    producer();
    producer();
    producer();
    producer();
    consumer();
    consumer();
    consumer();
    consumer();
    consumer();
    consumer();
    consumer();
    consumer();

    for (auto& thr : thrs) {
        thr.join();
    }

    auto end = std::chrono::steady_clock::now();
    std::cout << std::chrono::duration_cast<std::chrono::milliseconds>(end - now).count() << "\n";
    std::cout << produce_count << " " << real_produce_count << " " << consume_count << "\n";
}

void slist_test() {
    slist<int> q;
    std::vector<std::thread> thrs;
    std::atomic_int produce_count = 0;
    std::atomic_int real_produce_count = 0;
    std::atomic_int consume_count = 0;
    std::atomic_int total_count = 1000000 * 3;

    auto producer = [&q, &produce_count, &total_count, &real_produce_count]() {
        while (produce_count.fetch_add(1) < total_count) {
            q.push(produce_count);
            real_produce_count++;
        }
    };

    auto consumer = [&q, &consume_count, &total_count]() {
        while (true) {
            if (consume_count == total_count) {
                break;
            }

            if (!q.empty()) {
                q.pop();
                consume_count++;
            }
        }
    };

    auto now = std::chrono::steady_clock::now();
    producer();
    consumer();

    auto end = std::chrono::steady_clock::now();
    std::cout << std::chrono::duration_cast<std::chrono::milliseconds>(end - now).count() << "\n";
    std::cout << produce_count << " " << real_produce_count << " " << consume_count << "\n";
}

void mutex_slist_test() {
    slist<int> q;
    std::mutex mu;
    std::vector<std::thread> thrs;
    std::atomic_int produce_count = 0;
    std::atomic_int real_produce_count = 0;
    std::atomic_int consume_count = 0;
    std::atomic_int total_count = 1000000 * 3;

    auto producer = [&thrs, &q, &produce_count, &total_count, &real_produce_count, &mu]() {
        thrs.emplace_back(std::thread([&q, &produce_count, &total_count, &real_produce_count, &mu]() {
            while (produce_count.fetch_add(1) < total_count) {
                mu.lock();
                q.push(produce_count);
                mu.unlock();
                real_produce_count++;
            }
        }));
    };

    auto consumer = [&thrs, &q, &consume_count, &total_count, &mu]() {
        thrs.emplace_back(std::thread([&q, &consume_count, &total_count, &mu]() {
            while (true) {
                if (consume_count == total_count) {
                    break;
                }

                mu.lock();
                if (!q.empty()) {
                    q.pop();
                    mu.unlock();
                    consume_count++;
                } else {
                    mu.unlock();
                }
            }
        }));
    };

    auto now = std::chrono::steady_clock::now();
    producer();
    producer();
    producer();
    producer();
    producer();
    producer();
    producer();
    producer();
    consumer();
    consumer();
    consumer();
    consumer();
    consumer();
    consumer();
    consumer();
    consumer();

    for (auto& thr : thrs) {
        thr.join();
    }

    auto end = std::chrono::steady_clock::now();
    std::cout << std::chrono::duration_cast<std::chrono::milliseconds>(end - now).count() << "\n";
    std::cout << produce_count << " " << real_produce_count << " " << consume_count << "\n";
}

void time_pool_test() {
    print_withtime("begin");

    async_time_pool p;
    std::atomic_int count = 0;

    std::thread thr0([&p, &count]() {
        for (int i = 0; i < 1000000; i++) {
            p.async_add_timer(10, [&count]() {
                count++;
            });
        }
    });

    std::thread thr([&p, &count]() {
       for (int i = 0; i < 1000000; i++) {
           p.async_add_timer(10, [&count]() {
               count++;
           });
       }
    });

    std::thread thr2([&p, &count]() {
        for (int i = 0; i < 1000000; i++) {
            p.async_add_timer(10, [&count]() {
                count++;
            });
        }
    });

    std::thread thr3([&p, &count]() {
        p.update();
    });

    print_withtime(std::string("beg count:") + std::to_string(p.timer_count()));

    thr0.join();
    thr.join();
    thr2.join();
    thr3.join();

    print_withtime(std::string("end count:") + std::to_string(count));
    print_withtime(std::string("end count:") + std::to_string(p.timer_count()));
}

#include "common/work_steal_queue.hpp"
void work_steal_queue_test() {
    WorkStealingQueue<int> q(8);
    while (true) {
        if (!q.try_push(1)) {
            break;
        }
    }
    std::cout << q.size() << "\n";
}

void condition_variable_test() {
    std::mutex mu;
    std::condition_variable cond;
    moodycamel::ConcurrentQueue<int> queue;

    std::this_thread::sleep_for(std::chrono::milliseconds(1000));

    std::thread producer([&queue, &cond]() {
        print_withtime("begin");
        auto beg = std::chrono::steady_clock::now();
        for (int i = 0; i < 1000000; i++) {
            queue.enqueue(i);
            cond.notify_one();
        }
        auto end = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end - beg);
        print_withtime(std::string("end: ") + std::to_string(elapsed.count()));
    });

    for (int i = 0; i < 32; i++) {
        std::thread([&queue, &mu, &cond](){
            while (true) {
                int i = 0;
                if (!queue.try_dequeue(i)) {
                    std::unique_lock<std::mutex> task_lock(mu);
                    //std::chrono::milliseconds wait_t(50);
                    cond.wait(task_lock);
                }
            }
        }).detach();
    }

    producer.join();

    while (true) {
        std::this_thread::sleep_for(std::chrono::seconds(20*10));
        break;
    }
}

void cas_cqueue_test() {
    cas_cqueue<int> que(100);
    std::atomic_int produce_cnt = 0;
    std::atomic_int consume_cnt = 0;

    for (int i = 0; i < 1; i++) {
        std::thread([&que, &produce_cnt]{
            for (int j = 0; j < 10000; j++) {
                if (que.push(j)) {
                    produce_cnt++;
                }
            }
        }).detach();
    }

    for (int i = 0; i < 10; i++) {
        std::thread([&que, &consume_cnt]{
            int v = 0;
            while (true) {
                if (que.pop(v)) {
                    consume_cnt++;
                }
            }
        }).detach();
    }

    while (true) {
        std::cout << produce_cnt << " " << consume_cnt << "\n";
        std::this_thread::sleep_for(std::chrono::seconds(2));
    }
}

int main()
{
    enum test_type {
        t_work_steal_queue_test = 0,
        t_performance_base_test,
        t_performance_test2,
        t_performance_test3,
        t_time_pool_test,
        t_cgo_test,
        t_cgo_wait_test,
        t_condition_variable_test,
        t_chan_test,
        t_performance_copool_base_test,
        t_cas_cqueue_test,
    };

    switch (t_cas_cqueue_test) {
        case t_work_steal_queue_test:
            work_steal_queue_test();
            break;
        case t_performance_base_test:
            performance_base_test();
            break;
        case t_performance_test2:
            performance_test2();
            break;
        case t_performance_test3:
            performance_test3();
            break;
        case t_time_pool_test:
            time_pool_test();
            break;
        case t_cgo_test:
            cgo_test();
            break;
        case t_condition_variable_test:
            condition_variable_test();
            break;
        case t_cgo_wait_test:
            cgo_wait_test();
            break;
        case t_chan_test:
            chan_test();
            break;
        case t_performance_copool_base_test:
            performance_copool_base_test();
            break;
        case t_cas_cqueue_test:
            cas_cqueue_test();
            break;
        default:
            break;
    }

    //concurrentqueue_test();
    //slist_test();
    //mutex_slist_test();
    //atomic_test();
    //cqueue_test();
    //lock_test();
    //time_test();
    //cond_test();
    pause();
	std::cout << "finish\n";
	return 0;
}
