//
// Created by xiaoqj on 2023/5/16.
//

#pragma once

#include <grpc/support/log.h>
#include <grpcpp/grpcpp.h>
#include <string>
#include <memory>
#include <functional>
#include "co_grpc/runner/runner.h"
#include "co_grpc/server/calldata.h"
#include "co_grpc/server/server_builder.h"
#include "co_grpc/log.h"

namespace co_grpc {

template<class T, class Runner>
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
        new ServerData<Request, Response, Runner>(cq_, method, oncall);
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
        new ServerStreamReader<Request, Response, Runner>(cq_, method, oncall);
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
        new ServerStreamWriter<Request, Response, Runner>(cq_, method, oncall);
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
        new ServerStreamReaderWriter<Request, Response, Runner>(cq_, method, oncall);
    }
};

template<typename T>
class AsyncServer : public BaseServer<T, NormalRunner> {
public:
    AsyncServer() {
    }
};

template<typename T>
class CoServer : public BaseServer<T, CoRunner> {
public:
    CoServer() {
    }
};

// alias
template<typename T>
using Server = CoServer<T>;

}











