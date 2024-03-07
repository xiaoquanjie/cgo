// 文件生成时间: 2024-03-07 16:01:07.3081597 +0800 CST m=+0.446326201
#pragma once

#include "AITemperamentConfigReader.h"
#include "ErrorCodeKitReader.h"

namespace sheetcfg {
    extern AITemperamentConfigReader gAITemperamentConfigReader;
    extern ErrorCodeKitReader gErrorCodeKitReader;
    bool LoadAllReader(const std::string&);
}
