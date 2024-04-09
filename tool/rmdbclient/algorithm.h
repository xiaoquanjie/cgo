//
// Created by xiaoqj on 2023/7/10.
//

#pragma once

#include <string>
#include <vector>
#include <unordered_map>

namespace google::protobuf {
    class Message;
}

namespace rmdbclient {

    void split(const std::string &source, const std::string &separator, std::vector<std::string> &array);

    uint16_t checkSum16(const std::string& data);

    std::string& base64Encode(const std::string& data, std::string& output);

    std::string& base64Decode(const std::string& data, std::string& output);

    bool Proto2BsonBytes(const ::google::protobuf::Message *msg, std::string& output);

    bool BsonBytes2Proto(const std::string& data, ::google::protobuf::Message *msg);

    void* Kv2Bson(const std::unordered_map<std::string, std::string>& value);

    void freeBson(void*);

    const char* bsonBytes(void*, int* len);

    bool BsonBytes2Kv(const std::string& data, std::unordered_map<std::string, std::string>& value);

}
