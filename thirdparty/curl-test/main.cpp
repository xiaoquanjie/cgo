//
// Created by xiaoqj on 2024/1/9.
//

#include <iostream>
#include <string.h>
#include "cgo/cgo.h"
#include "co_curl/co_curl.h"

int main() {
    // 为了测试是否成功hook了libcurl，把cgo的并发线程限制1个
    cgoprocs(1);
    cgo_global_hook(true);

    for (int i = 0; i < 1; i++) {
        go []() {
            // 为了测试是否成功hook了libcurl，服务器需要延迟1秒以上响应请求
            auto response = co_curl::Get("192.168.204.61:10000/hello");
            if (!response) {
                std::cout << "error\n";
                return;
            }

            if (response->Ok()) {
                std::cout << response->value << "\n";
            } else {
                std::cout << response->curl_code << "\n";
            }
            msleep(10);
        };
    }

    // 如果成功hook住了，则gosleep会正常相隔一秒不断的输出
    go [] {
        while (true) {
            std::cout << "gosleep\n";
            gosleep(1000);
        }
    };

    while (true) {
        msleep(1);
    }
    return 0;
}