//
// Created by xiaoqj on 2023/5/26.
//

#pragma once

#include <string>

class HeapProfiler {
public:
    static void Switch(const std::string& file_name);

    static void Dump();
private:
    static void Start(const std::string& file_name);

    static void Stop();
};
