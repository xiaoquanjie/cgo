//
// Created by xiaoqj on 2024/1/9.
//

#pragma once

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