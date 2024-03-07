// 文件首次生成于时间: 2024-03-07 15:06:58.9987936 +0800 CST m=+0.232030101 
#pragma once

#include "sheet_reader.h"
#include "AITemperamentConfig.pb.h"

namespace sheetcfg {
    // 索引类型
    using AITemperamentConfigKey=SheetKey<>;
    // 模板参数分别为：索引类型，自定义结构类型，默认类型(proto生成), proto表类型
    using AITemperamentConfigBaseReader=SheetReader<AITemperamentConfigKey, AITemperamentConfig, AITemperamentConfig, AITemperamentConfigSheet>;

    struct AITemperamentConfigReader : public AITemperamentConfigBaseReader {
    protected:
        //解析器实现
        bool parser(AITemperamentConfigKey& key, AITemperamentConfig& newitem, AITemperamentConfig& item) {
            newitem = item;
            key.key1 = newitem.id();
            return true;
        }
    };
}