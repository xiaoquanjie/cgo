// 文件首次生成于时间: 2024-03-07 15:06:59.2512697 +0800 CST m=+0.484493301 
#pragma once

#include "sheet_reader.h"
#include "ErrorCodeKit.pb.h"

namespace sheetcfg {
    // 索引类型
    using ErrorCodeKitKey=SheetKey<>;
    // 模板参数分别为：索引类型，自定义结构类型，默认类型(proto生成), proto表类型
    using ErrorCodeKitBaseReader=SheetReader<ErrorCodeKitKey, ErrorCodeKit, ErrorCodeKit, ErrorCodeKitSheet>;

    struct ErrorCodeKitReader : public ErrorCodeKitBaseReader {
    protected:
        //解析器实现
        bool parser(ErrorCodeKitKey& key, ErrorCodeKit& newitem, ErrorCodeKit& item) {
            newitem = item;
            key.key1 = newitem.id();
            return true;
        }
    };
}