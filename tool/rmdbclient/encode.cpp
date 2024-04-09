//
// Created by xiaoqj on 2023/7/10.
//

#include "encode.h"
#include "algorithm.h"
#include "rmdb/meta.pb.h"
#include <vector>
#include <algorithm>

namespace rmdbclient {

    std::string &meta2Key(const rmdb::MetaData *meta, std::string &output) {
        std::vector<std::string> keys;
        for (auto& kv: meta->keys()) {
            keys.push_back(kv.first);
        }

        std::sort(keys.begin(), keys.end());

        std::string key1, key2;
        for (size_t i = 0; i < keys.size(); i++) {
            auto &mp = meta->keys();
            auto &val = mp.find(keys[i])->second;

            if (i == 0) {
                key1 = keys[i];
                std::string tmp;
                base64Encode(val, tmp);
                key2 = tmp;
            } else {
                key1 += "-" + keys[i];
                std::string tmp;
                base64Encode(val, tmp);
                key2 += "-" + tmp;
            }
        }

        output = meta->db() + ":" + meta->table() + ":" + key1 + ":" + key2;
        return output;
    }

    uint32_t CalcSlot(const std::string &key, uint32_t slots) {
        auto cc = checkSum16(key);
        auto slot = (uint32_t) cc % slots;
        return slot + 1;
    }


}
