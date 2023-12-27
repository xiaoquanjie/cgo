//
// Created by xiaoqj on 2023/5/26.
//

#pragma once

#include <functional>
#include <stdarg.h>
#include <stdio.h>

namespace co_grpc {

struct logger {
    std::function<void(const char*)> log_cb = [](const char* data) {
        printf("%s\n", data);
    };

    static logger* Instance() {
        static logger l;
        return &l;
    }
};

inline void log(const char* format, ...) {
    char buf[1024] = { 0 };
    va_list ap;
    va_start(ap, format);
    vsprintf(buf, format, ap);

    logger::Instance()->log_cb(buf);
}

// 设置日志接口, 要求是线程安全的
inline void SetSafeLogFunc(std::function<void(const char*)> cb) {
    logger::Instance()->log_cb = cb;
}

}
