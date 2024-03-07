// 文件生成时间: 2024-03-07 16:01:07.3081597 +0800 CST m=+0.446326201
#include "AllSheetReader.h"

namespace sheetcfg {
   AITemperamentConfigReader gAITemperamentConfigReader;
   ErrorCodeKitReader gErrorCodeKitReader;

    bool LoadAllReader(const std::string& dir) {
       if (gAITemperamentConfigReader.Load((dir+"/AITemperamentConfig.data").c_str()) == false) return false;
       if (gErrorCodeKitReader.Load((dir+"/ErrorCodeKit.data").c_str()) == false) return false;
       return true;    }
}