//
// Created by xiaoqj on 2023/5/26.
//

#pragma once

#include <string>

class HeapProfiler {
public:
    // 切换式调用：即一次为开，再次为关
    static void Switch(const std::string& file_name);

    static void Dump();
private:
    static void Start(const std::string& file_name);

    static void Stop();
};
