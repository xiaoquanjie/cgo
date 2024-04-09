//
// Created by xiaoqj on 2023/7/10.
//

#include "algorithm.h"
#include <google/protobuf/util/json_util.h>
#include <bson.h>

namespace rmdbclient {

    static const std::string base64_chars =
            "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
            "abcdefghijklmnopqrstuvwxyz"
            "0123456789+/";

    void split(const std::string &source,
               const std::string &separator,
               std::vector<std::string> &array) {
        array.clear();
        std::string::size_type start = 0;
        while (true) {
            std::string::size_type pos = source.find(separator, start);
            if (pos == std::string::npos) {
                std::string sub = source.substr(start, source.size());
                array.push_back(sub);
                break;
            }

            std::string sub = source.substr(start, pos - start);
            start = pos + separator.size();
            array.push_back(sub);
        }
    }

    uint16_t checkSum16(const std::string &data) {
        const uint16_t crcPoly = 0x1021;
        uint16_t crc = 0xFFFF; // 初始值为0xFFFF.

        for (auto b: data) {
            crc = crc ^ ((uint16_t) b << 8); // 将当前字节加入CRC中.
            for (int i = 0; i < 8; i++) {
                if ((crc & 0x8000) != 0) {
                    crc = (crc << 1) ^ crcPoly; // 如果CRC最高位为1，执行异或操作.
                } else {
                    crc = crc << 1; // 如果CRC最高位为0，仅执行移位操作.
                }
            }
        }

        return crc;
    }

    bool isBase64(unsigned char c);

    std::string &base64Encode(const std::string &data, std::string &output) {
        output.clear();
        auto bytes_to_encode = data.c_str();
        auto in_len = data.length();

        int i = 0;
        int j = 0;
        unsigned char char_array_3[3];  // store 3 byte of bytes_to_encode
        unsigned char char_array_4[4];  // store encoded character to 4 bytes

        while (in_len--) {
            char_array_3[i++] = *(bytes_to_encode++);  // get three bytes (24 bits)
            if (i == 3) {
                // eg. we have 3 bytes as ( 0100 1101, 0110 0001, 0110 1110) --> (010011, 010110, 000101, 101110)
                char_array_4[0] = (char_array_3[0] & 0xfc) >> 2; // get first 6 bits of first byte,
                char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0)
                        >> 4); // get last 2 bits of first byte and first 4 bit of second byte
                char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0)
                        >> 6); // get last 4 bits of second byte and first 2 bits of third byte
                char_array_4[3] = char_array_3[2] & 0x3f; // get last 6 bits of third byte

                for (i = 0; (i < 4); i++)
                    output += base64_chars[char_array_4[i]];
                i = 0;
            }
        }

        if (i) {
            for (j = i; j < 3; j++)
                char_array_3[j] = '\0';

            char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
            char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
            char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);

            for (j = 0; (j < i + 1); j++)
                output += base64_chars[char_array_4[j]];

            while ((i++ < 3))
                output += '=';

        }

        return output;
    }

    std::string &base64Decode(const std::string &data, std::string &output) {
        output.clear();
        auto &encoded_string = data;
        size_t in_len = encoded_string.size();

        int i = 0;
        int j = 0;
        int in_ = 0;
        unsigned char char_array_4[4], char_array_3[3];

        while (in_len-- && (encoded_string[in_] != '=') && isBase64(encoded_string[in_])) {
            char_array_4[i++] = encoded_string[in_];
            in_++;
            if (i == 4) {
                for (i = 0; i < 4; i++)
                    char_array_4[i] = base64_chars.find(char_array_4[i]) & 0xff;

                char_array_3[0] = (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4);
                char_array_3[1] = ((char_array_4[1] & 0xf) << 4) + ((char_array_4[2] & 0x3c) >> 2);
                char_array_3[2] = ((char_array_4[2] & 0x3) << 6) + char_array_4[3];

                for (i = 0; (i < 3); i++)
                    output += char_array_3[i];
                i = 0;
            }
        }

        if (i) {
            for (j = 0; j < i; j++)
                char_array_4[j] = base64_chars.find(char_array_4[j]) & 0xff;

            char_array_3[0] = (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4);
            char_array_3[1] = ((char_array_4[1] & 0xf) << 4) + ((char_array_4[2] & 0x3c) >> 2);

            for (j = 0; (j < i - 1); j++) output += char_array_3[j];
        }

        return output;
    }

    bool isBase64(unsigned char c) {
        return (isalnum(c) || (c == '+') || (c == '/'));
    }

    bool Proto2BsonBytes(const ::google::protobuf::Message *msg, std::string &output) {
        std::string jsonData;
        google::protobuf::util::JsonPrintOptions options;
        options.add_whitespace = true;
        options.always_print_primitive_fields = true;
        options.preserve_proto_field_names = true;
        auto status = google::protobuf::util::MessageToJsonString(*msg, &jsonData, options);
        if (!status.ok()) {
            return false;
        }

        bson_error_t berr;
        auto b = bson_new_from_json((const uint8_t *) jsonData.c_str(), jsonData.length(), &berr);
        if (!b) {
            return false;
        }

        uint32_t len = 0;
        auto bsonData = bson_destroy_with_steal(b, true, &len);

        if (len > 0) {
            output.append((const char *) bsonData, len);
        }

        bson_free(bsonData);
        return true;
    }

    bool BsonBytes2Proto(const std::string &data, ::google::protobuf::Message *msg) {
        auto b = bson_new_from_data((const uint8_t *) data.c_str(), data.length());
        if (!b) {
            return false;
        }

        size_t len = 0;
        auto j = bson_as_json(b, &len);

        auto status = google::protobuf::util::JsonStringToMessage(j, msg);
        if (!status.ok()) {
            return false;
        }

        bson_destroy(b);
        bson_free(j);
        return true;
    }

    void *Kv2Bson(const std::unordered_map<std::string, std::string> &value) {
        auto b = bson_new();
        for (auto &kv: value) {
            BCON_APPEND(b, kv.first.c_str(), kv.second.c_str());
        }
        return (void *) b;
    }

    void freeBson(void *b) {
        bson_destroy((bson_t *) b);
    }

    const char *bsonBytes(void *b, int *len) {
        auto buf = bson_get_data((bson_t *) b);
        *len = ((bson_t *) b)->len;
        return (char *) buf;
    }

    bool BsonBytes2Kv(const std::string &data, std::unordered_map<std::string, std::string> &value) {
        auto b = bson_new_from_data((const uint8_t *) data.c_str(), data.size());
        if (!b) {
            return false;
        }

        bson_iter_t iter;
        bson_iter_init(&iter, b);

        while (bson_iter_next(&iter)) {
            auto key = bson_iter_key(&iter);
            auto val = bson_iter_utf8(&iter, NULL);
            value[std::string(key)] = std::string(val);
        }

        bson_destroy(b);
        return true;
    }

}
