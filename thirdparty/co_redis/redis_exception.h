//
// Created by xiaoqj on 2024/1/9.
//

#pragma once

#include <memory>
#include <string>

namespace co_redis {
    struct RedisException {
    protected:
        std::shared_ptr<std::string> what_;

    public:
        RedisException() {}

        RedisException(const char* what) {
            what_.reset(new std::string(what));
        }

        RedisException(const std::string& what) {
            what_.reset(new std::string(what));
        }

        std::string What()const {
            if (!what_)
                return std::string();
            return *what_;
        }

        bool Empty()const {
            return (!what_);
        }
    };
}

#undef M_ERR_REDIS_TOO_MANY_CONNECTION
#define M_ERR_REDIS_TOO_MANY_CONNECTION ("over redis connection count limit")

#undef M_ERR_REDIS_CONNECT_FAIL
#define M_ERR_REDIS_CONNECT_FAIL ("redis connect fail")

#undef M_ERR_REDIS_NOT_DEFINED
#define M_ERR_REDIS_NOT_DEFINED ("redis not defined error")

#undef M_ERR_REDIS_AUTH_FAIL
#define M_ERR_REDIS_AUTH_FAIL ("redis auth fail")

#undef M_ERR_REDIS_INVALID_CONNECTION
#define M_ERR_REDIS_INVALID_CONNECTION ("invalid redis connection")

#undef M_ERR_REDIS_CONNECT_CLOSED
#define M_ERR_REDIS_CONNECT_CLOSED ("redis connection closed")