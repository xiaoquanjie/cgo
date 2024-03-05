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

namespace co_grpc {

template<class T>
class BaseServer : public IServer {
public:
    typedef typename T::AsyncService AsyncServiceType;

protected:
    AsyncServiceType service_;
    ::grpc::ServerCompletionQueue* cq_;

    BaseServer(const BaseServer&)  = delete;
    BaseServer& operator= (const BaseServer&) = delete;

public:
    BaseServer() {
        this->cq_ = DefSrvBuilder()->GetQueue();
    }

    ::grpc::Service* GetService() override {
        return &service_;
    }

    template<class Request, class Response, class GRPC_FUNC, class ON_CALL>
    void RegMethod(GRPC_FUNC func, ON_CALL oncall) {
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

        auto data = new ServerUnaryData<Request, Response, Responder, Method, ON_CALL>(this->cq_, method, oncall);
        data->doRequest();
    }

    template<class Request, class Response, class GRPC_FUNC, class ON_CALL>
    void RegCSMethod(GRPC_FUNC func, ON_CALL oncall) {
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

        auto data = new ServerCSData<Request, Response, Responder, Method, ON_CALL>(this->cq_, method, oncall);
        data->doRequest();
    }

    template<class Request, class Response, class GRPC_FUNC, class ON_CALL>
    void RegSSMethod(GRPC_FUNC func, ON_CALL oncall) {
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

        auto data = new ServerSSData<Request, Response, Responder, Method, ON_CALL>(this->cq_, method, oncall);
        data->doRequest();
    }

    // 双向流
    template<class Request, class Response, class GRPC_FUNC, class ON_CALL>
    void RegBSMethod(GRPC_FUNC func, ON_CALL oncall) {
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

        auto data = new ServerDSData<Request, Response, Responder, Method, ON_CALL>(this->cq_, method, oncall);
        data->doRequest();
    }
};

template<typename T>
class CoServer : public BaseServer<T> {
public:
    CoServer() {
    }
};

// alias
template<typename T>
using Server = CoServer<T>;

}











