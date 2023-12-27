//
// Created by xiaoqj on 2023/5/16.
//

#pragma once

#include <grpc/support/log.h>
#include <grpcpp/grpcpp.h>
#include <string>
#include <memory>
#include <functional>
#include "../runner/runner.h"
#include "./calldata.h"
#include "../log.h"

namespace co_grpc {

template<class T, class Runner = CoRunner>
class Server {
public:
    typedef T ServiceType;
    typedef typename T::AsyncService AsyncServiceType;
    typedef Runner RunnerType;

    Server() {
        cq_ = nullptr;
    }

    Server(::grpc::ServerCompletionQueue* cq) {
        cq_ = cq;
    }

    virtual ~Server() {}

    AsyncServiceType* GetService() {
        return &service_;
    }

    // 子类需要实现
    virtual void InitMethod() = 0;

    void SetQueue(::grpc::ServerCompletionQueue* cq) {
        cq_ = cq;
    }

    // 调用此接口前一定要先调用listen
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

    // 调用此接口前一定要先调用listen
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

    // 调用此接口前一定要先调用listen
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

protected:
    Server(const Server&)  = delete;
    Server& operator= (const Server&) = delete;

private:
    AsyncServiceType service_;
    ::grpc::ServerCompletionQueue* cq_;
};


}











