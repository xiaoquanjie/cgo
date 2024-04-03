//
// Created by xiaoqj on 2023/5/16.
//

#pragma once

#include <grpc/support/log.h>
#include <grpcpp/grpcpp.h>
#include <string>
#include <memory>
#include <functional>
#include "server_builder.h"

namespace cogrpc {

template<class T>
class BaseServer : public IServer {
public:
    typedef typename T::AsyncService AsyncServiceType;

protected:
    AsyncServiceType service_;
    ::grpc::ServerCompletionQueue* cq_ = nullptr;

public:
    BaseServer(const BaseServer&)  = delete;
    BaseServer& operator= (const BaseServer&) = delete;

    BaseServer() {
        this->cq_ = DefSrvBuilder()->GetQueue();
    }

    ::grpc::Service* GetService() override {
        return &service_;
    }

    template<class Request, class Response, class GRPC_FUNC, class ObjMethod, class Obj>
    void RegMethod(GRPC_FUNC func, ObjMethod oncall, Obj* obj) {
        if (!cq_) {
            assert(false);
            return;
        }

        auto method = std::bind(func,
                                &service_,
                                std::placeholders::_1,
                                std::placeholders::_2,
                                std::placeholders::_3,
                                std::placeholders::_4,
                                std::placeholders::_5,
                                std::placeholders::_6);

        typedef decltype(method) Method;
        typedef ::grpc::ServerAsyncResponseWriter<Response> Responder;

        auto data = new ServerUnaryData<Request, Response, Responder, Method, ObjMethod, Obj>
                (this->cq_, method, oncall, obj);
        data->doRequest();
    }

    template<class Request, class Response, class GRPC_FUNC, class ObjMethod, class Obj>
    void RegCSMethod(GRPC_FUNC func, ObjMethod oncall, Obj* obj) {
        if (!cq_) {
            assert(false);
            return;
        }

        auto method = std::bind(func,
                                &service_,
                                std::placeholders::_1,
                                std::placeholders::_2,
                                std::placeholders::_3,
                                std::placeholders::_4,
                                std::placeholders::_5);

        typedef decltype(method) Method;
        typedef ::grpc::ServerAsyncReader<Response, Request> Responder;

        auto data = new ServerCSData<Request, Response, Responder, Method, ObjMethod, Obj>
                (this->cq_, method, oncall, obj);
        data->doRequest();
    }

    template<class Request, class Response, class GRPC_FUNC, class ObjMethod, class Obj>
    void RegSSMethod(GRPC_FUNC func, ObjMethod oncall, Obj* obj) {
        if (!cq_) {
            assert(false);
            return;
        }

        auto method = std::bind(func,
                                &service_,
                                std::placeholders::_1,
                                std::placeholders::_2,
                                std::placeholders::_3,
                                std::placeholders::_4,
                                std::placeholders::_5,
                                std::placeholders::_6);

        typedef decltype(method) Method;
        typedef ::grpc::ServerAsyncWriter<Response> Responder;

        auto data = new ServerSSData<Request, Response, Responder, Method, ObjMethod, Obj>
                (this->cq_, method, oncall, obj);
        data->doRequest();
    }

    template<class Request, class Response, class GRPC_FUNC, class ObjMethod, class Obj>
    void RegBSMethod(GRPC_FUNC func, ObjMethod oncall, Obj* obj) {
        if (!cq_) {
            assert(false);
            return;
        }

        auto method = std::bind(func,
                                &service_,
                                std::placeholders::_1,
                                std::placeholders::_2,
                                std::placeholders::_3,
                                std::placeholders::_4,
                                std::placeholders::_5);

        typedef decltype(method) Method;
        typedef ::grpc::ServerAsyncReaderWriter<Response, Request> Responder;

        auto data = new ServerDSData<Request, Response, Responder, Method, ObjMethod, Obj>
                (this->cq_, method, oncall, obj);
        data->doRequest();
    }
};

template<typename T>
class CoServer : public BaseServer<T> {
public:
    CoServer() = default;
};

// alias
template<typename T>
using Server = CoServer<T>;

}











