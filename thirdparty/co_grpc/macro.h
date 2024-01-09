//
// Created by xiaoqj on 2023/5/19.
// 定义grpc会用到的宏
//

#pragma once

#include "./server/server.h"
#include "./server/calldata.h"
#include "./client/client.h"
#include "./client/calldata.h"
#include "./client/client_builder.h"
#include "./traits.h"

//////////////////////////////////////////////// 服务器相关的宏 begin //////////////////////////////////////
// 注册服务器一元类方法：类内部调用
#define GRPC_SRV_CO_UNARY_METHOD(rpcMethod, request, response) \
{                                                              \
    typedef decltype(*this) RefSelf;                           \
    typedef std::remove_reference<RefSelf>::type Self;         \
    auto __impl_func__ = &Self::rpcMethod;                     \
    auto __on_call_ =                                          \
    std::bind(__impl_func__, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3); \
    this->RegMethod<request, response>(&Self::AsyncServiceType::Request##rpcMethod, __on_call_);         \
    co_grpc::log("grpc RegMethod: %s.%s", typeid(Self).name(), #rpcMethod);      \
}

// 注册服务器方法：客户端流模式，类内部调用
#define GRPC_SRV_CO_CS_METHOD(rpcMethod, request, response) \
{                                                           \
    typedef decltype(*this) RefSelf; \
    typedef std::remove_reference<RefSelf>::type Self;                 \
    auto __impl_func__ = &Self::rpcMethod; \
    auto __on_call_ =             \
        std::bind(__impl_func__, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3); \
    auto __async_method__ = &Self::AsyncServiceType::Request##rpcMethod;     \
    this->RegCSMethod<request, response>(__async_method__, __on_call_);                              \
    co_grpc::log("grpc RegCSMethod: %s.%s", typeid(Self).name(), #rpcMethod); \
}

// 注册服务器方法: 服务器流模式，类内部调用
#define GRPC_SRV_CO_SS_METHOD(rpcMethod, request, response) \
{\
    typedef decltype(*this) RefSelf;              \
    typedef std::remove_reference<RefSelf>::type Self;                 \
    auto __impl_func__ = &Self::rpcMethod;                                      \
    auto __on_call_ = \
            std::bind(__impl_func__, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3); \
    auto __async_method__ = &Self::AsyncServiceType::Request##rpcMethod;     \
    this->RegSSMethod<request, response>(__async_method__, __on_call_);\
    co_grpc::log("grpc RegSSMethod: %s.%s", typeid(Self).name(), #rpcMethod); \
}

// 注册服务器方法: 双流模式，类内部调用
#define GRPC_SRV_CO_BS_METHOD(rpcMethod, request, response) \
{ \
    typedef decltype(*this) RefSelf; \
    typedef std::remove_reference<RefSelf>::type Self;                 \
    auto __impl_func__ = &Self::rpcMethod; \
    auto __on_call_ = \
            std::bind(__impl_func__, this, std::placeholders::_1, std::placeholders::_2); \
    auto __async_method__ = &Self::AsyncServiceType::Request##rpcMethod;       \
    this->RegBSMethod<request, response>(__async_method__, __on_call_);\
    co_grpc::log("grpc RegBSMethod: %s.%s", typeid(Self).name(), #rpcMethod);  \
}

//////////////////////////////////////////////// 服务器相关的宏 end //////////////////////////////////////

//////////////////////////////////////////////// 服务器stream相关的宏 begin //////////////////////////////////////
#define GRPC_SERVER_STREAM_READER(request, response) co_grpc::ServerStreamReader<request, response, co_grpc::CoRunner>
#define GRPC_SERVER_STREAM_WRITER(request, response) co_grpc::ServerStreamWriter<request, response, co_grpc::CoRunner>
#define GRPC_SERVER_STREAM_READER_WRITER(request, response) co_grpc::ServerStreamReaderWriter<request, response, co_grpc::CoRunner>
//////////////////////////////////////////////// 服务器stream相关的宏 end   //////////////////////////////////////

///////////////////////////////////////////////  客户端返回值相关的宏 begin ///////////////////////////////
// 客户端同步接口返回值：客户端流模式
#define GRPC_CLIENT_SYNC_WRITER(request) std::unique_ptr<::grpc::ClientWriter<request>>
// 客户端异步接口返回值：客户端流模式
#define GRPC_CLIENT_ASYNC_WRITER(request, response) std::shared_ptr<co_grpc::ClientStreamWriter<request, response>>
// 客户端协程接口返回值：客户端流模式
#define GRPC_CLIENT_CO_WRITER(request, response) std::shared_ptr<co_grpc::CoClientStreamWriter<request, response>>

// 客户端同步接口返回值: 服务端流模式
#define GRPC_CLIENT_SYNC_READER(response) std::unique_ptr<::grpc::ClientReader<response>>
// 客户端异步接口返回值：服务端流模式
#define GRPC_CLIENT_ASYNC_READER(request, response) std::shared_ptr<co_grpc::ClientStreamReader<request, response>>
// 客户端协程接口返回值：服务端流模式
#define GRPC_CLIENT_CO_READER(request, response) std::shared_ptr<co_grpc::CoClientStreamReader<request, response>>

// 客户端同步接口返回值：双流模式
#define GRPC_CLIENT_SYNC_RW(request, response) std::unique_ptr<::grpc::ClientReaderWriter<request, response>>
// 客户端异步接口返回值：双流模式
#define GRPC_CLIENT_ASYNC_RW(request, response) std::shared_ptr<co_grpc::ClientStreamReaderWriter<request, response>>
#define GRPC_CLIENT_CO_RW(request, response) std::shared_ptr<co_grpc::CoClientStreamReaderWriter<request, response>>
///////////////////////////////////////////////  客户端返回值相关的宏 end  ////////////////////////////////


//////////////////////////////////////////////// 客户端相关的宏 begin /////////////////////////////////////
// 协程客户端一元类接口
#define GRPC_CLIENT_CO_UNARY_METHOD(rpcMethod, request, response) \
::grpc::Status rpcMethod(std::shared_ptr<::grpc::ClientContext> ctx, const request& req, response* res) { \
    return this->Send<request, response>(&Stub::PrepareAsync##rpcMethod, ctx, req, res); \
}

// 协程客户端客户端流接口
#define GRPC_CLIENT_CO_CS_METHOD(rpcMethod, request, response) \
GRPC_CLIENT_CO_WRITER(request, response) rpcMethod(std::shared_ptr<::grpc::ClientContext> ctx, response* rsp) { \
    return this->CSend<request, response>(&Stub::PrepareAsync##rpcMethod, ctx, rsp); \
}

// 协程客户端服务端流接口
#define GRPC_CLIENT_CO_SS_METHOD(rpcMethod, request, response) \
GRPC_CLIENT_CO_READER(request, response) rpcMethod(std::shared_ptr<::grpc::ClientContext> ctx, const request& req) { \
    return this->SSend<request, response>(&Stub::PrepareAsync##rpcMethod, ctx, req);      \
}

// 协程客户端双流接口
#define GRPC_CLIENT_CO_BS_METHOD(rpcMethod, request, response) \
GRPC_CLIENT_CO_RW(request, response) rpcMethod(std::shared_ptr<::grpc::ClientContext> ctx) {\
    return this->BSend<request, response>(&Stub::PrepareAsync##rpcMethod, ctx); \
}

//////////////////////////////////////////////// 客户端相关的宏 end //////////////////////////////////////





// 错误码判断
#define GRPC_MSG(status) status.error_message()
#define GRPC_OK(status) status.ok()
#define GRPC_CODE(status) status.error_code()
#define GRPC_CONNECTION_FAIL(status) !status.ok() && status.error_code() == 14
#define GRPC_TIMEOUT(status) !status.ok() && status.error_code() == 4

// 状态码构造
#define GRPC_STATUS(code, msg) ::grpc::Status(::grpc::StatusCode(code), msg)