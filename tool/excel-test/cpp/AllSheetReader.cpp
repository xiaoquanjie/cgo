// 文件生成时间: 2024-03-08 12:08:40.6996527 +0800 CST m=+0.065848201

#include "AllSheetReader.h"

namespace sheetcfg {
    AITemperamentConfigReader gAITemperamentConfigReader;
    ErrorCodeKitReader gErrorCodeKitReader;

    bool LoadAllReader(const std::string& dir) {
        if (gAITemperamentConfigReader.Load((dir+"/AITemperamentConfig.data").c_str()) == false) return false;
        if (gErrorCodeKitReader.Load((dir+"/ErrorCodeKit.data").c_str()) == false) return false;
        return true;
    }
}