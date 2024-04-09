//
// Created by xiaoqj on 2024/4/9.
//

#pragma once

#include <mutex>
#include <iostream>
#include <iomanip>
#include <thread>
#include <cgo.h>

inline void print_withtime(const char* msg) {
    static std::mutex mu;
    std::unique_lock<std::mutex> lock(mu);

    std::chrono::system_clock::time_point now = std::chrono::system_clock::now();
    std::time_t now_c = std::chrono::system_clock::to_time_t(now);
    std::tm* timeinfo = std::localtime(&now_c);

    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;

    char buffer[80];
    std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", timeinfo);

    std::cout << buffer << '.' << std::setfill('0') << std::setw(3) << ms.count() << " ";
    std::cout << std::this_thread::get_id() << " " << (int64_t)cgocoid() << " " << msg << "\n";
}

inline void print_withtime(const std::string& msg) {
    print_withtime(msg.c_str());
}

