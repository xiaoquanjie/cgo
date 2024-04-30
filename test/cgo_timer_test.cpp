//
// Created by xiaoqj on 2024/4/29.
//

#include <gtest/gtest.h>
#include <atomic>
#include "print.h"
#include <cgo/common/timer.h>
#include <cgo/common/time_pool.h>

int count = 0;
TEST(cgo_timer_test, 1) {
    const int total = 1000*1000*10;
    print_withtime("begin");
    auto t = timer::NewTimer([](uint64_t id, uint64_t payload) {
        count++;
    });

    for (auto idx = 0; idx < total; idx++) {
        t->add(10, idx);
    }

    print_withtime("add end");
    while (t->count() > 0) {
        t->update();
    }

    print_withtime("end");
    EXPECT_TRUE(count == total);
    delete t;
}

TEST(cgo_timer_test, 2) {
    const int total = 1000*1000*10;
    int count = 0;
    print_withtime("begin");
    time_pool t;

    for (auto idx = 0; idx < total; idx++) {
        t.add_timer(10, [&count] {
            count++;
        });
    }

    print_withtime("add end");
    while (t.timer_count() > 0) {
        t.update();
    }

    print_withtime("end");
    EXPECT_TRUE(count == total);
}

TEST(cgo_timer_test, 3) {
    print_withtime("begin");

    auto t = timer::NewTimer([](uint64_t id, uint64_t payload) {
        print_withtime("payload:" + std::to_string(payload));
    });

    for (int i = 0; i < 10; i++) {
        msleep(2000);
        t->add(1000, i);
        while (t->count() > 0) {
            t->update();
        }
    }
    delete t;
}

TEST(cgo_timer_test, 4) {
    print_withtime("begin");

    auto t = timer::NewTimer([](uint64_t id, uint64_t payload) {
        print_withtime("payload:" + std::to_string(payload));
    });

    for (int i = 0; i < 100; i++) {
        t->add(1+i, i);
    }
    while (t->count() > 0) {
        t->update();
    }
    delete t;
}

TEST(cgo_timer_test, 5) {
    print_withtime("begin");

    auto t = timer::NewTimer([](uint64_t id, uint64_t payload) {
        print_withtime("payload:" + std::to_string(payload));
    });
    for (int idx = 0; idx < (60*60-1); idx++) {
        assert(t->add((1+idx)*1000, idx+1) > 0);
    }

    msleep(2000);
    while (t->count() > 0) {
        t->update();
    }
    delete t;
}

int main(int argc, char **argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}