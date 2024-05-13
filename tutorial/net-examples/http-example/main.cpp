//
// Created by xiaoqj on 2024/4/1.
//

#include "cgo/thirdparty/conet/conet.h"
#include <iostream>

int main() {
    auto ok = conet::ListenHttpAndServe("tcp", "0.0.0.0", 50052, [](conet::HttpResponse* writer, conet::HttpRequest* in) {
        writer->SetHeader("Content-Type", "text/html; charset=UTF-8");
        std::string data = "hello";
        writer->Write(data.c_str(), (int)data.length());
    });
    if (!ok) {
        return -1;
    }
    std::cout << "quit...\n";
    return 0;
}