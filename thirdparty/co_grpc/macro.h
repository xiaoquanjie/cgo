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
#include "./type_traits.hpp"

template<class T>
struct ClientWriterTraits {
    typedef T type;
};

template<class T>
struct ClientWriterTraits<::grpc::ClientWriter<T> > {
    typedef T type;
};

template<class T>
struct ClientReaderTraits {
    typedef T type;
};

template<class T>
struct ClientReaderTraits<::grpc::ClientReader<T> > {
    typedef T type;
};

template<class T1, class T2>
struct ClientReaderWriterTraits {
    typedef T1 first_arg;
    typedef T2 second_arg;
};

template<class T1, class T2>
struct ClientReaderWriterTraits<::grpc::ClientReaderWriter<T1, T2>, int> {
    typedef T1 first_arg;
    typedef T2 second_arg;
};

// 注册std::function类型的回调：func
#define __REG_GRPC_SRV_FUNCTION(srv, rpcName, func) \
{                                            \
    typedef typename decltype(func)::first_argument_type  ref_first_arg;    \
    typedef typename decltype(func)::second_argument_type ref_second_arg;   \
    typedef std::remove_reference<ref_first_arg>::type    first_arg;        \
    typedef std::remove_reference<ref_second_arg>::type   second_arg;       \
    co_grpc::log("grpc RegMethod: %s.%s", ##srv, #rpcName);   \
    srv.RegMethod<first_arg, second_arg>(&decltype(srv)::AsyncServiceType::Request##rpcName, func);    \
}

// 注册c函数类型的回调
#define __REG_GRPC_SRV_METHOD1(srv, rpcName, func) \
{                                           \
    typedef typename function_traits<decltype(func)>::arg_type<0>::type ref_first_arg;  \
    typedef typename function_traits<decltype(func)>::arg_type<1>::type ref_second_arg; \
    typedef std::remove_reference<ref_first_arg>::type    first_arg;                    \
    typedef std::remove_reference<ref_second_arg>::type   second_arg;                   \
    std::function<::grpc::Status(ref_first_arg, ref_second_arg, ::grpc::ServerContext&)> f = \
        std::bind(func, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3); \
    co_grpc::log("grpc RegMethod: %s.%s", ##srv, #rpcName);   \
    srv.RegMethod<first_arg, second_arg>(&decltype(srv)::AsyncServiceType::Request##rpcName, f);      \
}

// 注册成员函数类型回调
#define __REG_GRPC_SRV_METHOD2(srv, rpcName, func, obj) \
{                                                 \
    typedef typename function_traits<decltype(func)>::arg_type<0>::type ref_first_arg;  \
    typedef typename function_traits<decltype(func)>::arg_type<1>::type ref_second_arg; \
    typedef std::remove_reference<ref_first_arg>::type    first_arg;                    \
    typedef std::remove_reference<ref_second_arg>::type   second_arg;                   \
    std::function<::grpc::Status(ref_first_arg, ref_second_arg, ::grpc::ServerContext&)> f = \
        std::bind(func, obj, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3);    \
    co_grpc::log("grpc RegMethod: %s.%s", ##srv, #rpcName);           \
    srv.RegMethod<first_arg, second_arg>(&decltype(srv)::AsyncServiceType::Request##rpcName, f);      \
}

#define __REG_GRPC_SRV_UNARY_METHOD(rpcMethod, RequestType, ResponseType) \
{ \
    typedef decltype(*this) RefSelf;                                      \
    typedef std::remove_reference<RefSelf>::type Self;                   \
    auto __impl_func__ = &Self::rpcMethod;                                  \
    auto __on_call_ = \
        std::bind(__impl_func__, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3); \
    co_grpc::log("grpc RegMethod: %s.%s", typeid(Self).name(), #rpcMethod);  \
    this->RegMethod<RequestType, ResponseType>(&Self::AsyncServiceType::Request##rpcMethod, __on_call_);  \
}

// 客户端流模式内部注册宏
#define __REG_GRPC_SRV_CS_METHOD(rpcMethod, RequestType, ResponseType) \
{   \
    typedef decltype(*this) RefSelf; \
    typedef std::remove_reference<RefSelf>::type Self;                 \
    auto __impl_func__ = &Self::rpcMethod; \
    auto __on_call_ =             \
        std::bind(__impl_func__, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3); \
    co_grpc::log("grpc RegCSMethod: %s.%s", typeid(Self).name(), #rpcMethod);\
    auto __async_method__ = &Self::AsyncServiceType::Request##rpcMethod;     \
    this->RegCSMethod<RequestType, ResponseType>(__async_method__, __on_call_); \
}

// 服务器流模式内部注册宏
#define __REG_GRPC_SRV_SS_METHOD(rpcMethod, RequestType, ResponseType) \
{\
    typedef decltype(*this) RefSelf;              \
    typedef std::remove_reference<RefSelf>::type Self;                 \
    auto __impl_func__ = &Self::rpcMethod;                                      \
    auto __on_call_ = \
            std::bind(__impl_func__, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3); \
    co_grpc::log("grpc RegSSMethod: %s.%s", typeid(Self).name(), #rpcMethod);\
    auto __async_method__ = &Self::AsyncServiceType::Request##rpcMethod;     \
    this->RegSSMethod<RequestType, ResponseType>(__async_method__, __on_call_); \
}

// 服务器双向流内部注册宏
#define __REG_GRPC_SRV_BS_METHOD(rpcMethod, RequestType, ResponseType) \
{ \
    typedef decltype(*this) RefSelf; \
    typedef std::remove_reference<RefSelf>::type Self;                 \
    auto __impl_func__ = &Self::rpcMethod; \
    auto __on_call_ = \
            std::bind(__impl_func__, this, std::placeholders::_1, std::placeholders::_2); \
    co_grpc::log("grpc RegBSMethod: %s.%s", typeid(Self).name(), #rpcMethod);\
    auto __async_method__ = &Self::AsyncServiceType::Request##rpcMethod;       \
    this->RegBSMethod<RequestType, ResponseType>(__async_method__, __on_call_);\
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////

#define __HANDLE_GRPC_CLIENT_SYNC_UNARY_METHOD(rpcMethod, context, request, response) \
{ \
    typedef co_grpc::type_traits<decltype(*this)>::value_type Self; \
    typedef co_grpc::type_traits<decltype(request)>::value_type RequestType; \
    typedef co_grpc::type_traits<decltype(response)>::value_type ResponseType; \
    return this->Send<RequestType, ResponseType>(&Self::Stub::rpcMethod, context, request, response); \
}

#define __HANDLE_GRPC_CLIENT_SYNC_CS_METHOD(rpcMethod, context, response) \
{   \
    typedef co_grpc::type_traits<decltype(*this)>::value_type Self; \
    typedef typename function_traits<decltype(&Self::rpcMethod)>::result_type ReturnType;  \
    typedef typename ReturnType::element_type ElementType;   \
    typedef typename ClientWriterTraits<ElementType>::type RequestType;  \
    typedef co_grpc::type_traits<decltype(response)>::value_type ResponseType;             \
    return std::move(this->SyncCSend<RequestType, ResponseType>(&Self::Stub::rpcMethod, context, response));  \
}

#define __HANDLE_GRPC_CLIENT_SYNC_SS_METHOD(rpcMethod, context, request) \
{                                                                        \
    typedef co_grpc::type_traits<decltype(*this)>::value_type Self;      \
    typedef co_grpc::type_traits<decltype(request)>::value_type RequestType;  \
    typedef typename function_traits<decltype(&Self::rpcMethod)>::result_type ReturnType; \
    typedef typename ReturnType::element_type ElementType;               \
    typedef typename ClientReaderTraits<ElementType>::type ResponseType; \
    return std::move(this->SSend<RequestType, ResponseType>(&Self::Stub::rpcMethod, context, request)); \
}

#define __HANDLE_GRPC_CLIENT_SYNC_BS_METHOD(rpcMethod, context) \
{ \
    typedef co_grpc::type_traits<decltype(*this)>::value_type Self; \
    typedef typename function_traits<decltype(&Self::rpcMethod)>::result_type ReturnType; \
    typedef typename ReturnType::element_type ElementType; \
    typedef typename ClientReaderWriterTraits<ElementType, int>::first_arg    RequestType;  \
    typedef typename ClientReaderWriterTraits<ElementType, int>::second_arg   ResponseType; \
    return std::move(this->BSend<RequestType, ResponseType>(&Self::Stub::rpcMethod, context)); \
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////

#define __HANDLE_GRPC_CLIENT_ASYNC_UNARY_METHOD(rpcMethod, context, request) \
{ \
    typedef co_grpc::type_traits<decltype(*this)>::value_type Self; \
    typedef co_grpc::type_traits<decltype(request)>::value_type RequestType; \
    typedef co_grpc::type_traits<decltype(response)>::value_type ResponseType; \
    return this->Send<RequestType, ResponseType>(&Self::Stub::rpcMethod, context, request, response); \
}


//////////////////////////////////////////////// 服务器相关的宏 begin //////////////////////////////////////
// 注册服务器方法：回调为std::function类型
#define REG_GRPC_SRV_FUNCTION(srv, rpcName, func) __REG_GRPC_SRV_FUNCTION(srv, rpcName, func)
// 注册服务器方法：回调为c函数或者类成员方法
#define REG_GRPC_SRV_METHOD(srv, rpcName, ...) \
PRIVATE_MACRO_CHOOSE_HELPER(__REG_GRPC_SRV_METHOD, COUNT_MACRO_VAR_ARGS(__VA_ARGS__)) (srv, rpcName, __VA_ARGS__)
// 注册服务器一元类方法：类内部调用
#define REG_GRPC_SRV_UNARY_METHOD(rpcMethod, RequestType, ResponseType) __REG_GRPC_SRV_UNARY_METHOD(rpcMethod, RequestType, ResponseType)
// 注册服务器方法：客户端流模式，类内部调用
#define REG_GRPC_SRV_CS_METHOD(rpcMethod, RequestType, ResponseType) __REG_GRPC_SRV_CS_METHOD(rpcMethod, RequestType, ResponseType)
// 注册服务器方法: 服务器流模式，类内部调用
#define REG_GRPC_SRV_SS_METHOD(rpcMethod, RequestType, ResponseType) __REG_GRPC_SRV_SS_METHOD(rpcMethod, RequestType, ResponseType)
// 注册服务器方法: 双流模式，类内部调用
#define REG_GRPC_SRV_BS_METHOD(rpcMethod, RequestType, ResponseType) __REG_GRPC_SRV_BS_METHOD(rpcMethod, RequestType, ResponseType)
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
// 客户端一元类接口
#define HANDLE_GRPC_CLIENT_SYNC_UNARY_METHOD(rpcMethod, context, request, response) __HANDLE_GRPC_CLIENT_SYNC_UNARY_METHOD(rpcMethod, context, request, response)
// 客户端同步接口宏：客户端流模式
#define HANDLE_GRPC_CLIENT_SYNC_CS_METHOD(rpcMethod, context, response) __HANDLE_GRPC_CLIENT_SYNC_CS_METHOD(rpcMethod, context, response)
// 客户端同步服务器流接口
#define HANDLE_GRPC_CLIENT_SYNC_SS_METHOD(rpcMethod, context, request) __HANDLE_GRPC_CLIENT_SYNC_SS_METHOD(rpcMethod, context, request)
// 客户端同步双流接口
#define HANDLE_GRPC_CLIENT_SYNC_BS_METHOD(rpcMethod, context) __HANDLE_GRPC_CLIENT_SYNC_BS_METHOD(rpcMethod, context)

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