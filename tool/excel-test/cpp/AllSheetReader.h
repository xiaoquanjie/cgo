// 文件生成时间: 2024-03-07 16:20:13.3991498 +0800 CST m=+0.475979601
#pragma once

#include "AITemperamentConfigReader.h"
#include "ErrorCodeKitReader.h"

namespace sheetcfg {
    extern AITemperamentConfigReader gAITemperamentConfigReader;
    extern ErrorCodeKitReader gErrorCodeKitReader;
    bool LoadAllReader(const std::string&);
}
