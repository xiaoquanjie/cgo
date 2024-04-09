//
// Created by xiaoqj on 2023/7/10.
//

#pragma once

#include "rmdb/rmdb.pb.h"

namespace rmdbclient {

    std::string &meta2Key(const rmdb::MetaData *meta, std::string &key);

    uint32_t CalcSlot(const std::string &key, uint32_t slots);

}
