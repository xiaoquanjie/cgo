//
// Created by xiaoqj on 2024/4/1.
//

#include "cgo/thirdparty/conet/conet.h"
#include <cgo/thirdparty/coredis/coredis.h>
#include <iostream>

int main() {
    auto ok = conet::ListenHttpAndServe("tcp", "0.0.0.0", 50052, [](conet::HttpResponse* writer, conet::HttpRequest* in) {
        // 扩大协程空间
        cgostackful(1024*32) {
            writer->SetHeader("Content-Type", "text/html; charset=UTF-8");
            try {
                coredis::RedisPool::SetMaxConnection(50);
                //auto conn = coredis::RedisPool::GetConnection("192.168.204.81", "", 6379);
                //conn.Set("mykey", "myvalue");
                auto conn = coredis::RedisPool::GetConnection("192.168.102.26", "rwSlXLlwgaqvi4pSGrW3", 6379);
                conn.Set("test_string", "my_test_string");
                writer->Write("hello", 5);
            } catch(const std::runtime_error& e) {
                std::cout << "error:" << e.what() << "\n";
                writer->Write("error", 5);
            }
        };
    });
    if (!ok) {
        return -1;
    }
    std::cout << "quit...\n";
    return 0;
}