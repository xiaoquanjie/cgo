//
// Created by xiaoqj on 2024/4/9.
//

#include <gtest/gtest.h>
#include <atomic>
#include "print.h"

TEST(cgo_performance3_test, 1) {
    cgo::WaitGroup wg;
    std::atomic_int count = 0;
    int loopcount = 1000000;

    for (int i = 0; i < loopcount; i++) {
        wg.Add(1);
        go [&wg, &count] {
            count++;
            wg.Done();
            gowait(1000*20);
        };
    }

    wg.Wait();
    std::cout << count << "\n";
}

int main(int argc, char **argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}