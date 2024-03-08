// 文件生成时间: 2024-03-08 12:08:40.6996527 +0800 CST m=+0.065848201

#pragma once

#include "AITemperamentConfigReader.h"
#include "ErrorCodeKitReader.h"

namespace sheetcfg {
    extern AITemperamentConfigReader gAITemperamentConfigReader;
    extern ErrorCodeKitReader gErrorCodeKitReader;
    bool LoadAllReader(const std::string&);
}
