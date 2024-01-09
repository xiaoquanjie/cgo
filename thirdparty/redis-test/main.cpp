//
// Created by xiaoqj on 2024/1/9.
//

#include <iostream>
#include <hiredis/hiredis.h>
#include <string.h>
#include "cgo/cgo.h"
#include "co_redis/co_redis.h"

int main() {
    //co_redis::RedisPool::SetMaxConnection(2);

    for (int i = 0; i < 1; i++) {
        go []() {
            try {
                auto c = co_redis::RedisPool::GetConnection("192.168.102.26", "rwSlXLlwgaqvi4pSGrW3", 6379);
                std::cout << "connect ok\n";

                std::cout << "expire:" << c.Expire("mystring", 100) << "\n";

                std::cout << "del:" << c.Del({"mystring", "yes"}) << "\n";

            } catch (co_redis::RedisException& e) {
                std::cout << e.What() << "\n";
            }

            msleep(10);
        };
    }

    while (true) {
        msleep(1);
    }
    return 0;
}