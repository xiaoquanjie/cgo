//
// Created by xiaoqj on 2024/4/9.
//

#include <gtest/gtest.h>
#include <atomic>
#include "print.h"

TEST(cgo_mutex_test, 1) {
    cgo::WaitGroup wg;
    cgo::mutex mu;

    int loop_count = 1000000;
    int count = 0;

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

int main(int argc, char **argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}