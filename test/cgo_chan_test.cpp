//
// Created by xiaoqj on 2024/4/9.
//

#include <gtest/gtest.h>
#include <atomic>
#include "print.h"

void test(int bufsize) {
    cgo::WaitGroup wg;
    wg.Add(2);

    int loopcount = 1000000;
    int count = 0;

    typedef int data_type;
    auto ch = makechan<data_type>(bufsize);
    go [ch, &wg, loopcount] {
        for (int i = 0; i < loopcount; i++) {
            ch << i;
        }
        wg.Done();
    };

    go [ch, &wg, &count, loopcount] {
        data_type d;
        while (ch >> d) {
            count++;
            if (count >= loopcount) {
                break;
            }
        }
        wg.Done();
    };

    wg.Wait();
    closechan(ch);
    EXPECT_EQ(loopcount, count);
}

TEST(cgo_chan_test, 1) {
    test(0);
}

TEST(cgo_chan_test, 2) {
    test(10);
}

TEST(cgo_chan_test, 3) {
    test(50);
}

int main(int argc, char **argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}