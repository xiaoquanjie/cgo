#include <iostream>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <string>
#include <vector>
#include <iomanip>
#include <sstream>
#include "../cgo/cgo.h"

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
    std::cout << std::this_thread::get_id() << " " << (int64_t)cgocoid() << " " << msg << "\n";
}

void print_withtime(const std::string& msg) {
    print_withtime(msg.c_str());
}

void pause2() {
    while (true) {
        std::this_thread::sleep_for(std::chrono::seconds(200));
        break;
    }

    print_withtime("try to stop");
}

void cgo_test() {
    //cgoprocs(1);
    cgocore(2);

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

    cgo::cgo_print_debug_info();
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
                    gowait(1000);
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

#include "../cgo/common/time_pool.h"
void time_test() {
    async_time_pool p;
    std::thread([&p]() {
        for (int i = 0; i < 10; i++) {
            p.add_timer(30, [i]() {
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

    pause2();
}

#include "../cgo/common/circle_queue.h"
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
    typedef std::string data_type;
    cgo::chan<data_type> ch = makechan<data_type>(2);

    for (int i= 0; i < 500; i++) {
        go [ch]() {
            while (true) {
                //msleep(5000);
                data_type data;
                if (ch >> data) {
                    std::ostringstream oss;
                    oss << data;
                    print_withtime(std::string("recv:") + oss.str());
                } else {
                    print_withtime("recv over");
                    break;
                }
            }
        };
    }

    while (true) {
        std::string input;
        std::cout << "input:";
        std::cin >> input;

        if (input == "stop") {
            closechan(ch);
            break;
        }

        std::istringstream iis(input);
        data_type data;
        iis >> data;

        ch << data;
    }

    print_withtime("over");
}

void chan_performance_test() {
    /* 1000000次数据，16个协程 800ms
     * golang 测试：
     *  无缓存：300ms
     *  100缓存：100ms
     * */

    cgo::cgo_print_debug_info();
    cgo::chan<int> ch;
    ch = makechan<int>(1);

    int total_count = 100000;//1000000;
    int count = 0;
    int concurrent = 16;

    //////////////// begin /////////////////////
    auto beg = std::chrono::steady_clock::now();
    for (int i = 0; i < total_count; i++) {
        go [ch, &total_count, &count] {
            ch << 1;
            count++;
            ch >> channull;
        };
    }

    while (count < total_count) {
        msleep(1);
    }

    print_withtime(std::to_string(count));
    auto end = std::chrono::steady_clock::now();
    // 结果是1000多毫秒10w
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end - beg);
    print_withtime(std::string("cgo chan in multi coroutine: ") + std::to_string(elapsed.count()));
    //////////////// end /////////////////////

    //////////////// begin /////////////////////
    beg = std::chrono::steady_clock::now();
    count = 0;
    go [ch, &total_count] {
        for (int i = 0; i < total_count; i++) {
            ch << 1;
        }
    };
    go [ch, &total_count, &count] {
        while (true) {
            ch >> channull;
            count++;
            if (count >= total_count) {
                break;
            }
        }
    };

    while (count < total_count) {
        msleep(1);
    }
    print_withtime(std::to_string(count));
    end = std::chrono::steady_clock::now();
    // 结果是77多毫秒10w
    elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end - beg);
    print_withtime(std::string("cgo chan in read/write coroutine: ") + std::to_string(elapsed.count()));
    //////////////// end /////////////////////

    go [ch] {
        print_withtime(std::to_string(ch.use_count()));
        if (!(ch >> channull)) {
            print_withtime("over");
        }
    };

    closechan(ch);
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

#include "../cgo/coroutine/coroutine.h"
void performance_base_test() {
    /*
     *  100w次平均700毫秒, 这个已是系统协程库的切换性能极致
     * */
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

        auto end = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end - beg);
        print_withtime(std::string("base test end: ") + std::to_string(elapsed.count()));
        std::cout << "==============\n";
    }
}

#include "../cgo/coroutine/coroutine_adapter.h"
void performance_base_test2() {
    /*  use mincoro
     *  100w次平均1100毫秒
     * */
    for (int j = 0; j < 3; j++) {
        print_withtime("base test2 begin");
        std::atomic_int count = 0;
        auto beg = std::chrono::steady_clock::now();
        const int total_count = 1000000;

        for (int i = 0; i < total_count; i++) {
            cgo::coro_adapter::run_co([&count]() {
                count++;
            });
        }

        while (count != total_count) {
        }

        auto end = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end - beg);
        print_withtime(std::string("base 2 end: ") + std::to_string(elapsed.count()));
        std::cout << "==============\n";
    }
}

void performance_copool_base_test() {
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
     *      自由线程: 200~220ms
     * */

    for (int j = 0; j < 3; j++) {
        cgo::WaitGroup wg;
        print_withtime("begin");
        std::atomic_int count = 0;
        auto beg = std::chrono::steady_clock::now();
        const int total_count = 1000000;

        auto add_beg = std::chrono::steady_clock::now();
        for (int i = 0; i < total_count; i++) {
            wg.Add(1);
            go [&count, &wg]() {
                count++;
                wg.Done();
            };
        }
        auto add_end = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(add_end - add_beg);
        print_withtime(std::string("add end: ") + std::to_string(elapsed.count()));

        wg.Wait();

        auto end = std::chrono::steady_clock::now();
        elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end - beg);
        print_withtime(std::string("end: ") + std::to_string(elapsed.count()));
        std::cout << "==============\n";
    }

    cgo::cgo_print_debug_info();
}

void performance_test3() {
    //cgoprocs(1);
    go []() {
        //print_withtime("start");

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
                std::this_thread::sleep_for(std::chrono::microseconds(10));
            }

            auto end = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end - beg);
            print_withtime(std::string("end: ") + std::to_string(elapsed.count()));
            std::cout << "==============\n";
        }
    };
}

#include "cgo/common/spinlock.h"
void atomic_test() {
    int total = 1000000;
    int count = 0;
    int thrs = 20;
    auto beg = std::chrono::steady_clock::now();
    std::atomic_flag flag;
    flag.clear();
    std::mutex mu;
    folly::MicroSpinLock spinlock;
    spinlock.init();

    for (int i = 0; i < thrs; i++) {
        std::thread([&] {
            while (true) {
//                unsigned int backoff = 1;
//                while (flag.test_and_set(std::memory_order_relaxed)) {
//                    for (unsigned int i = 0; i < backoff; ++i) {
//                        asm volatile("pause\n" : : : "memory");
//                        //folly::asm_volatile_pause();
//                    }
//                    // 指数退避算法
//                    backoff = std::min(backoff * 2, 128u);
//                }

                //mu.lock();
                spinlock.lock();
                if (count != total) {
                    count++;
                }
                spinlock.unlock();
                //mu.unlock();
                //flag.clear(std::memory_order_relaxed);
                if (count >= total) {
                    break;
                }
            }
        }).detach();
    }

    while (count < total) {msleep(1);}
    auto end = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end - beg);
    print_withtime(std::string("end: ") + std::to_string(elapsed.count()));
    print_withtime(std::to_string(count));
}

#include "../cgo/common/concurrentqueue.h"
void concurrentqueue_test() {
    // 需要200多毫秒
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

    //async_time_pool* p = new async_time_pool;
    time_pool* p = new time_pool;

    std::atomic_int count = 0;
    int total = 1000000;
    auto beg = std::chrono::steady_clock::now();

    for (int i = 0; i < total; i++) {
          p->add_timer(1, [&count] {
              count++;
          });
    }

    //msleep(10);

    std::thread([p]() {
        while (true) {
            p->update();
        }
    }).detach();

    while (count < total) {
        //print_withtime(std::to_string(count));
    }

    auto end = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end - beg);
    print_withtime(std::string("time_pool_test: ") + std::to_string(elapsed.count()));
    print_withtime(std::to_string(p->timer_count()));
}

#include "../cgo/common/work_steal_queue.hpp"
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

#include <string.h>
void hook_connect_test() {
    cgoprocs(1);
    for (int i = 0; i < 2; i++) {
        go [i]() {
            int fd = socket(AF_INET, SOCK_STREAM, 0);
            if (fd == -1) {
                print_withtime("create socket error");
                return;
            }
            cgo_hook_fd(fd);
            struct sockaddr_in addr;
            memset(&addr, 0, sizeof(addr));
            addr.sin_family = AF_INET;
            addr.sin_port = htons(8080);
            if (inet_pton(AF_INET, "192.168.204.61", &addr.sin_addr) <= 0) {
                print_withtime("inet_pton error");
                return;
            }

            int ret = connect(fd, (struct sockaddr*)&addr, sizeof(addr));
            if (ret != 0) {
                print_withtime("connect error");
                close(fd);
                return;
            }

            while (true) {
                std::string data = "nihao" + std::to_string(i);
                auto cnt = send(fd, data.c_str(), data.length(), 0);
                if (cnt <= 0) {
                    break;
                }

                char buf[100];
                cnt = recv(fd, buf, 100, 0);
                if (cnt <= 0) {
                    break;
                }

                std::string rdata(buf, cnt);
                print_withtime(rdata.c_str());

                gowait(1000);
            }

            print_withtime("connection close");
            close(fd);
        };
    }
}

void hook_accept_test() {
    //cgoprocs(1);
    //cgo_global_hook(true);

    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd == -1) {
        print_withtime("create socket error");
        return;
    }

    /*int optval = 1;
    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (const char*) & optval, sizeof(optval)) == -1) {
        print_withtime("setsockopt error");
        return;
    }*/

    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(50052);  // 绑定的端口号
    server_addr.sin_addr.s_addr = INADDR_ANY;  // 监听所有可用的网络接口
    if (bind(fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) == -1) {
        print_withtime("bind error");
        return;
    }

    if (listen(fd, 10) == -1) {  // 允许最多 10 个连接请求等待处理
        print_withtime("listen error");
        return;
    }

    go [fd]() {
        cgo_hook_fd(fd);
        while (true) {
            struct sockaddr_in client_addr;
            socklen_t client_addrlen = sizeof(client_addr);
            int conn = accept(fd, (struct sockaddr*)&client_addr, &client_addrlen);
            if (conn == -1) {
                print_withtime("accept error");
                assert(false);
                continue;
            }

            print_withtime("a new connection");
            
            go [conn]() {
                cgo_hook_fd(conn);
                while (true) {
                    char buf[100] = {' ', 'r', 'e', 'a', 'd', ':'};
                    auto cnt = recv(conn, buf+6, 94, 0);
                    if (cnt <= 0) {
                        break;
                    }

                    buf[cnt+6] = '\0';
                    print_withtime(buf+1);

                    buf[0] = 'r';
                    buf[1] = 'e';
                    buf[2] = 'p';
                    buf[3] = 'l';
                    buf[4] = 'y';
                    buf[5] = ':';
                    send(conn, buf, cnt+6, 0);
                }

                print_withtime("connection close");
                close(conn);
            };
        }

        close(fd);
    };
}

void hook_udp_connect() {
    //cgoprocs(1);
    cgo_global_hook(true);

    for (int i = 0; i < 3; i++) {
        go[i]() {
            // 创建套接字
            int fd = socket(AF_INET, SOCK_DGRAM, 0);
            if (fd == -1) {
                print_withtime("create socket error");
                return;
            }

            while (true) {
                struct sockaddr_in serverAddr;
                // 设置服务器地址
                memset(&serverAddr, 0, sizeof(serverAddr));
                serverAddr.sin_family = AF_INET;
                serverAddr.sin_port = htons(8080);  // 设置服务器端口号
                if (inet_pton(AF_INET, "192.168.204.61", &(serverAddr.sin_addr)) <= 0) {
                    print_withtime("inet_pton error");
                    break;
                }

                // 发送数据
                std::string message = "Hello, server!";
                message += std::to_string(i);
                auto cnt = sendto(fd, message.c_str(), message.length(), 0, (struct sockaddr*)&serverAddr, sizeof(serverAddr));
                if (cnt <= 0) {
                    break;
                }

                char buffer[1024];
                memset(&serverAddr, 0, sizeof(serverAddr));
                socklen_t addrlen = sizeof(sockaddr_in);
                cnt = recvfrom(fd, buffer, 1024, 0, (struct sockaddr*)&serverAddr, &addrlen);
                if (cnt <= 0) {
                    break;
                }

                buffer[cnt] = '\0';
                print_withtime(buffer);
                gowait(1000);
            }

            print_withtime("connection close");
            close(fd);
        };
    }
}

#include "cgo/common/mpmcqueue.h"

void mutex_test() {
    cgo::cgo_print_debug_info();
    cgo::mutex mu;
    int total = 312;
    int count = 0;

    for (int i = 0; i < total; i++) {
        go [&, i] {
            while (true) {
                //msleep(10);
                mu.lock();
                //print_withtime(std::string("waiter count:") + std::to_string(mu.waiter()));
                print_withtime(std::string("coroutine") + std::to_string(i));
                mu.unlock();
            }
        };
    }

    //while(true);
    while (true) {
        //msleep(2000);
        mu.lock();
        print_withtime("main coroutine");
        //msleep(1000);
        mu.unlock();

    }
}

void mutex_performance_test() {
    // 消耗15毫秒
    std::mutex standard_mu;

    //================= standard mutex test =============
    auto beg = std::chrono::steady_clock::now();
    const int total_count = 1000000;
    for (int i = 0; i < total_count;) {
        standard_mu.lock();
        i++;
        standard_mu.unlock();
    }

    auto end = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end - beg);
    print_withtime(std::string("standard mutex in one thread: ") + std::to_string(elapsed.count()));
    //================= standard mutex test end=============

    //================= cgo mutex test =============
    // 消耗55毫秒
    cgo::mutex cgo_mu;
    go [&] {
        beg = std::chrono::steady_clock::now();
        for (int i = 0; i < total_count;) {
            cgo_mu.lock();
            //cgo_mu._lock.test_and_set();
            i++;
            //cgo_mu._lock.clear();
            cgo_mu.unlock();
        }

        end = std::chrono::steady_clock::now();
        elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end - beg);
        print_withtime(std::string("cgo mutex in one coroutine: ") + std::to_string(elapsed.count()));
    };
    //================= cgo mutex test end=============

    //================= standard mutex in multi thread test =============
}

#include "cgo/common/spinlock.h"
void mutex_performance_test2() {

    //================= standard mutex test =============
    std::mutex standard_mu; // 消耗55毫秒
    auto beg = std::chrono::steady_clock::now();
    int total_count = 1000000;
    int count = 0;
    int concurrent = 4;

    for (int i = 0; i < concurrent; i++) {
        std::thread([&] {
            while (true) {
                standard_mu.lock();
                if (count < total_count) {
                    count++;
                    standard_mu.unlock();
                } else {
                    standard_mu.unlock();
                    break;
                }
            }
        }).detach();
    }

    while (count != total_count) msleep(1);

    auto end = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end - beg);
    print_withtime(std::string("standard mutex in multi thread: ") + std::to_string(elapsed.count()));
    //================= standard mutex test end=============

    //msleep(2000);

    //================= cgo mutex test =============
    cgo::mutex cgo_mu; // 消耗950毫秒
    count = 0;
    beg = std::chrono::steady_clock::now();

    for (int i = 0; i < concurrent; i++) {
        go [&] {
            while (true) {
                cgo_mu.lock();
                if (count < total_count) {
                    count++;
                    cgo_mu.unlock();
                } else {
                    cgo_mu.unlock();
                    break;
                }
            }
        };
    }

    while (count != total_count) msleep(1);
    end = std::chrono::steady_clock::now();
    elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end - beg);
    print_withtime(std::string("cgo mutex in multi coroutine: ") + std::to_string(elapsed.count()));
    //================= cgo mutex test end=============

    //================= spinrwlock test =============
    folly::MicroSpinLock spinrw;
    count = 0;
    beg = std::chrono::steady_clock::now();

    for (int i = 0; i < concurrent; i++) {
        std::thread([&] {
            while (true) {
                spinrw.lock();
                if (count < total_count) {
                    count++;
                    spinrw.unlock();
                } else {
                    spinrw.unlock();
                    break;
                }
            }
        }).detach();
    }

    while (count != total_count) msleep(1);

    end = std::chrono::steady_clock::now();
    elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end - beg);
    print_withtime(std::string("spinrwlock in multi thread: ") + std::to_string(elapsed.count()));
    //================= spinrwlock test =============

}

void stdfunction_test() {
    print_withtime("stdfunction_test");
    auto beg = std::chrono::steady_clock::now();
    const int total_count = 1000000;
    moodycamel::ConcurrentQueue<std::function<void()>> que;
    for (int i = 0; i < total_count; i++) {
        que.enqueue([]{});
    }
    auto end = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end - beg);
    print_withtime(std::string("stdfunction_test: ") + std::to_string(elapsed.count()));
}

#include "cgo/common/semaphore.h"
void semaphore_test() {
    Semaphore sem;
    sem.post();
    sem.post();
    print_withtime("wait");
    sem.wait();
    sem.wait();
    sem.wait();
    print_withtime("over");
}

#include "cgo/common/mpmcqueue.h"
void simplelist_test() {
    // 消耗40毫秒
    rigtorp::MPMCQueue<int> que(1000000);

    auto beg = std::chrono::steady_clock::now();
    const int total_count = 1000000;

    for (int i = 0; i < 10; i++) {
        std::thread([&]{
            while (que.size() < total_count) {
                //li.push(1);
                que.push(1);
                //wsq.push()
                //cq.enqueue(1);
            }
        }).detach();
    }

    while (que.size() < total_count);
    std::cout << que.size() << "\n";
    auto end = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end - beg);
    print_withtime(std::string("simplelist_test end: ") + std::to_string(elapsed.count()));
}

void wait_performance_test() {
    //cgoprocs(1);
    cgo::cgo_print_debug_info();
    auto beg = std::chrono::steady_clock::now();
    const int total_count = 1000000;//1000000;
    std::atomic_int count = 0;

    for (int i = 0; i < total_count; i++) {
        go [&] {
            gowait(1);
            //msleep(1);
            count++;
            //print_withtime("yes");
        };
    }

    while (count < total_count) {
        //print_withtime(std::to_string(count));
        msleep(1);
    }
    auto end = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end - beg);
    print_withtime(std::string("wait_performance_test end: ") + std::to_string(elapsed.count()));
    print_withtime(std::to_string(count));
}

#include "cgo/coroutine/coroutine_adapter.h"
void co_alloc_test() {
    auto beg = std::chrono::steady_clock::now();
    const int total_count = 1000000;
    for (int i = 0; i < total_count; i++) {
        cgo::coroutine::create([]{}, 1024*64);
    }
    auto end = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end - beg);
    print_withtime(std::string("co_alloc_test end: ") + std::to_string(elapsed.count()));
}

#include "heap_profiler/heap_profiler.h"
int main()
{
    enum test_type {
        t_work_steal_queue_test = 0,
        t_performance_base_test,
        t_performance_base_test2,
        t_performance_test2,
        t_performance_test3,
        t_time_pool_test,
        t_cgo_test,
        t_cgo_wait_test,
        t_condition_variable_test,
        t_chan_test,
        t_performance_copool_base_test,
        t_atomic_test,
        t_hook_connect_test,
        t_hook_accept_test,
        t_hook_udp_connect,
        t_mutex_test,
        t_concurrentqueue_test,
        t_mutex_performance_test,
        t_mutex_performance_test2,
        t_chan_performance_test,
        t_cqueue_test,
        t_wait_performance_test,
        t_mutex_slist_test,
        t_co_alloc_test,
    };

    //HeapProfiler::Switch("profiler");

    switch (t_performance_test2) {
        case t_work_steal_queue_test:
            work_steal_queue_test();
            break;
        case t_performance_base_test:
            performance_base_test();
            break;
        case t_performance_base_test2:
            performance_base_test2();
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
        case t_atomic_test:
            atomic_test();
            break;
        case t_hook_connect_test:
            hook_connect_test();
            break;
        case t_hook_accept_test:
            hook_accept_test();
            break;
        case t_hook_udp_connect:
            hook_udp_connect();
            break;
        case t_mutex_test:
            mutex_test();
            break;
        case t_concurrentqueue_test:
            concurrentqueue_test();
            break;
        case t_mutex_performance_test:
            mutex_performance_test();
            break;
        case t_mutex_performance_test2:
            mutex_performance_test2();
            break;
        case t_chan_performance_test:
            chan_performance_test();
            break;
        case t_cqueue_test:
            cqueue_test();
            break;
        case t_wait_performance_test:
            wait_performance_test();
            break;
        case t_mutex_slist_test:
            mutex_slist_test();
            break;
        case t_co_alloc_test:
            co_alloc_test();
            break;
        default:
            break;
    }

    //HeapProfiler::Dump();

    //slist_test();

    //lock_test();
    //time_test();
    //cond_test();
    pause2();
	std::cout << "finish\n";
	return 0;
}
