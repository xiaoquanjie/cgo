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

                c.Set("mystring", "myvalue");

                std::cout << "expire:" << c.Expire("mystring", 100) << "\n";

                std::cout << "del:" << c.Del({"mystring", "yes"}) << "\n";

                std::cout << "setnx:" << c.SetNx("mystring", "myvalue", 15000) << "\n";

                std::cout << "setxx:" << c.SetXx("mystring", "myvalue", 15000) << "\n";

                std::string val;
                std::cout << "get:" << c.Get("mystring", val) << "\n";

                std::cout << "getset:" << c.GetSet("mystring", "myvalue2", val) << "\n";

                std::cout << "strlen:" << c.Strlen("mystring") << "\n";
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