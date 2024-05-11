//
// Created by xiaoqj on 2024/4/9.
//

#include <gtest/gtest.h>
#include <atomic>
#include <thread>
#include <list>
#include "print.h"

TEST(cgo_mutex_test, 1) {
    cgo::WaitGroup wg;
    cgo::mutex mu;

    int loop_count = 1000000;
    int count = 0;

    // golang的基准测试才花200多毫秒
    for (int i = 0 ; i < loop_count; i++) {
        wg.Add(1);
        // 这里性能表现不好.需要排查一下原因,可能是因为内存分配的原因.
        go [&mu, &count, &wg] {
            mu.lock();
            count++;
            mu.unlock();
            wg.Done();
        };
    }

    wg.Wait();
    EXPECT_EQ(loop_count, count);
}

TEST(standard_mutex_test, 1) {
    int loop_count = 1000000;
    int count = 0;

    std::mutex mu;
    std::list<std::thread> thrs;
    for (int i = 0; i < std::thread::hardware_concurrency(); i++) {
        thrs.push_back(std::move(std::thread([&mu, &count, loop_count]{
            while (true) {
                mu.lock();
                if (count == loop_count) {
                    mu.unlock();
                    break;
                }
                count++;
                mu.unlock();
            }
        })));
    }

    for (auto& th : thrs) {
        th.join();
    }

    EXPECT_EQ(loop_count, count);
}

TEST(cgo_mutex_test, 2) {
    int loop_count = 1000000;
    int count = 0;

    cgo::mutex mu;
    cgo::WaitGroup wg;

    for (int i = 0; i < std::thread::hardware_concurrency(); i++) {
        wg.Add(1);
        go [&mu, &count, loop_count, &wg] {
            while (true) {
                mu.lock();
                if (count == loop_count) {
                    mu.unlock();
                    break;
                }
                count++;
                mu.unlock();
            }
            wg.Done();
        };
    }

    wg.Wait();
    EXPECT_EQ(loop_count, count);
}

int main(int argc, char **argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}