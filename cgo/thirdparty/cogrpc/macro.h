//
// Created by xiaoqj on 2023/5/19.
// 定义grpc会用到的宏.
//

#pragma once

#include "server/server.h"
#include "client/client.h"

//////////////////////////////////////////////// 服务器相关的宏 begin. //////////////////////////////////////
// 注册服务器一元类方法,类内部调用.
#define GRPC_SRV_UNARY_METHOD(rpcMethod, request, response) \
{                                                              \
    typedef decltype(*this) RefSelf;                           \
    typedef std::remove_reference<RefSelf>::type Self;         \
    this->RegMethod<request, response>(&Self::AsyncServiceType::Request##rpcMethod, &Self::rpcMethod, this);  \
}

// 注册服务器方法,客户端流模式,类内部调用.
#define GRPC_SRV_CS_METHOD(rpcMethod, request, response) \
{                                                           \
    typedef decltype(*this) RefSelf; \
    typedef std::remove_reference<RefSelf>::type Self;                 \
    this->RegCSMethod<request, response>(&Self::AsyncServiceType::Request##rpcMethod, &Self::rpcMethod, this);    \
}

// 注册服务器方法:服务器流模式,类内部调用.
#define GRPC_SRV_SS_METHOD(rpcMethod, request, response) \
{\
    typedef decltype(*this) RefSelf;              \
    typedef std::remove_reference<RefSelf>::type Self;                 \
    this->RegSSMethod<request, response>(&Self::AsyncServiceType::Request##rpcMethod, &Self::rpcMethod, this);\
}

// 注册服务器方法:双流模式,类内部调用.
#define GRPC_SRV_BS_METHOD(rpcMethod, request, response) \
{ \
    typedef decltype(*this) RefSelf; \
    typedef std::remove_reference<RefSelf>::type Self;                 \
    this->RegBSMethod<request, response>(&Self::AsyncServiceType::Request##rpcMethod, &Self::rpcMethod, this);\
}

//////////////////////////////////////////////// 服务器相关的宏 end. //////////////////////////////////////

//////////////////////////////////////////////// 客户端相关的宏 begin. /////////////////////////////////////
// 协程客户端一元类接口.
#define GRPC_CLIENT_UNARY_METHOD(rpcMethod, request, response) \
::grpc::Status rpcMethod(std::shared_ptr<::grpc::ClientContext> ctx, const request& req, response* res) { \
    return this->Send<request, response>(&Stub::PrepareAsync##rpcMethod, std::move(ctx), req, res); \
}

// 协程客户端客户端流接口.
#define GRPC_CLIENT_CS_METHOD(rpcMethod, request, response) \
GRPC_CLIENT_WRITER(request, response) rpcMethod(std::shared_ptr<::grpc::ClientContext> ctx, response* rsp) { \
    return this->CSend<request, response>(&Stub::PrepareAsync##rpcMethod, std::move(ctx), rsp); \
}

// 协程客户端服务端流接口.
#define GRPC_CLIENT_SS_METHOD(rpcMethod, request, response) \
GRPC_CLIENT_READER(request, response) rpcMethod(std::shared_ptr<::grpc::ClientContext> ctx, const request& req) { \
    return this->SSend<request, response>(&Stub::PrepareAsync##rpcMethod, std::move(ctx), req);      \
}

// 协程客户端双流接口.
#define GRPC_CLIENT_BS_METHOD(rpcMethod, request, response) \
GRPC_CLIENT_RW(request, response) rpcMethod(std::shared_ptr<::grpc::ClientContext> ctx) {\
    return this->BSend<request, response>(&Stub::PrepareAsync##rpcMethod, std::move(ctx)); \
}

//////////////////////////////////////////////// 客户端相关的宏 end. //////////////////////////////////////

// 错误码判断.
#define GRPC_MSG(status) status.error_message()
#define GRPC_OK(status) status.ok()
#define GRPC_CODE(status) status.error_code()
#define GRPC_CONNECTION_FAIL(status) !status.ok() && status.error_code() == 14
#define GRPC_TIMEOUT(status) !status.ok() && status.error_code() == 4

// 状态码构造.
#define GRPC_STATUS(code, msg) ::grpc::Status(::grpc::StatusCode(code), msg)