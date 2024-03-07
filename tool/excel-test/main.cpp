//
// Created by xiaoqj on 2024/3/7.
//

#include "cpp/AllSheetReader.h"
#include <fstream>
#include <iostream>

int main() {
    auto filename = "./data/ErrorCodeKit.data";
    if (!sheetcfg::LoadAllReader("./data")) {
        std::cout << "load all reader error\n";
        return 0;
    }

    std::cout << sheetcfg::gErrorCodeKitReader.GetSheet()->ShortDebugString() << "\n";
    return 0;
}