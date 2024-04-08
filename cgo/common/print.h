/*----------------------------------------------------------------
// Copyright 2021
// All rights reserved.
//
// author: 404558965@qq.com (xiaoquanjie)
// github: https://github.com/xiaoquanjie/cgo
// Created by xiaoqj on 2023/11/01.
//----------------------------------------------------------------*/

#pragma once

#include <chrono>
#include <utility>
#include <ctime>

#ifndef M_CO_DEBUG_PRINT
#include <cstdio>
#define M_CO_DEBUG_PRINT co_printf //printf //
#endif

template<typename... Args>
void co_printf(const char* format, Args&&... args) {
    std::chrono::system_clock::time_point now = std::chrono::system_clock::now();
    std::time_t now_c = std::chrono::system_clock::to_time_t(now);
	std::tm* timeinfo = std::localtime(&now_c);

    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;

    char buffer[80];
    std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", timeinfo);

    printf("%s.%lld ", buffer, (long long int)ms.count());
    printf(format, args...);
}