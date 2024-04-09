//
// Created by xiaoqj on 2024/4/9.
//

#include <gtest/gtest.h>
#include <atomic>
#include "print.h"

TEST(cgo_wait_test, 1) {
    cgo::WaitGroup wg;
    wg.Add(1);
    go [&wg] {
        int loopcount = 100;
        for (int i = 0; i < loopcount; i++) {
            gowait(10);
        }
        wg.Done();
    };

    wg.Wait();
}

int main(int argc, char **argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}