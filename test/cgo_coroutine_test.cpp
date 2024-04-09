//
// Created by xiaoqj on 2024/4/9.
//

#include <gtest/gtest.h>
#include <atomic>
#include "print.h"

TEST(cgo_coroutine_test, 1) {
    cgoprocs(1);

    int loopcount = 1000;
    std::atomic_int count = 0;
    cgo::WaitGroup wg;

    for (int i = 0; i < loopcount; i++) {
        wg.Add(1);
        go [&count, &wg] {
            count++;
            wg.Done();
        };
    }

    wg.Wait();
    EXPECT_EQ(count, loopcount);
}

int main(int argc, char **argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}