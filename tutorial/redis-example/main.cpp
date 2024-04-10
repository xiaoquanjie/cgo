//
// Created by xiaoqj on 2024/4/1.
//

#include <cgo/thirdparty/coredis/coredis.h>
#include <cgo/cgo.h>
#include <iostream>

int main() {
    cgo::WaitGroup wg;
    for (int i = 0; i < 1; i++) {
        wg.Add(1);
        go [&wg] {
            try {
                std::cout << "begin\n";

                auto conn = coredis::RedisPool::GetConnection("192.168.102.26", "rwSlXLlwgaqvi4pSGrW3", 6379);
                conn.Set("test_string", "my_test_string");

                std::string val;
                conn.Get("test_string", &val);
                std::cout << val << "\n";

                conn.Del(std::string("test_string"));
            } catch(const std::runtime_error& e) {
                std::cout << "error:" << e.what() << "\n";
            }
            wg.Done();
        };
    }

    wg.Wait();
    return 0;
}