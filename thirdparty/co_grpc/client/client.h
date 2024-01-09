//
// Created by xiaoqj on 2023/5/16.
//

#pragma once

#include <grpc/support/log.h>
#include <grpcpp/grpcpp.h>
#include <grpcpp/completion_queue.h>
#include <string>
#include <memory>
#include <functional>
#include <chrono>
#include "./channel.h"
#include "./calldata.h"
#include "./client_builder.h"
#include "../log.h"
#include "../runner/runner.h"

namespace co_grpc {

template<typename T>
class BaseClient {
public:
    typedef typename T::Stub Stub;

protected:
    std::unique_ptr<Stub> stub_;

public:
    // 绑定通道
    void Bind(std::shared_ptr<::grpc::Channel> channel) {
        if (stub_) {
            return;
        }
        stub_ = T::NewStub(channel);
    }

    void Bind(const std::string& target, const std::string& lb_policy) {
        std::shared_ptr<::grpc::Channel> c = GetChannel(target, lb_policy);
        Bind(c);
    }

};

// 同步客户端
template<typename T>
class SyncClient : public BaseClient<T> {
public:
    typedef typename BaseClient<T>::Stub Stub;

protected:
    std::unique_ptr<Stub> stub_;

public:
    // 同步一元类发送接口
    template<class Request, class Response, class GRPC_FUNC>
    ::grpc::Status Send(GRPC_FUNC func, ::grpc::ClientContext* context, const Request& request, Response* response) {
        if (!stub_) {
            assert(false);
            return ::grpc::Status::CANCELLED;
        }

        Stub* stub = stub_.get();

        if (!context) {
            ::grpc::ClientContext ctx;
            context = &ctx;
            return (stub->*func)(context, request, response);
        } else {
            return (stub->*func)(context, request, response);
        }
    }

    // 同步客户端流发送接口
    template<class Request, class Response, class GRPC_FUNC>
    std::unique_ptr<::grpc::ClientWriter<Request>> CSend(GRPC_FUNC func, ::grpc::ClientContext* context, Response* response) {
        if (!stub_) {
            assert(false);
            return nullptr;
        }

        Stub* stub = stub_.get();

        if (!context) {
            ::grpc::ClientContext ctx;
            context = &ctx;
            std::unique_ptr<::grpc::ClientWriter<Request>> writer((stub->*func)(context, response));
            return std::move(writer);
        } else {
            std::unique_ptr<::grpc::ClientWriter<Request>> writer((stub->*func)(context, response));
            return std::move(writer);
        }
    }

    // 同步服务器流发送接口
    template<class Request, class Response, class GRPC_FUNC>
    std::unique_ptr<::grpc::ClientReader<Response>> SSend(GRPC_FUNC func, ::grpc::ClientContext* context, const Request& request) {
        if (!stub_) {
            assert(false);
            return nullptr;
        }

        Stub* stub = stub_.get();

        if (!context) {
            ::grpc::ClientContext ctx;
            context = &ctx;
            std::unique_ptr<::grpc::ClientReader<Response>> reader((stub->*func)(context, request));
            return std::move(reader);
        } else {
            std::unique_ptr<::grpc::ClientReader<Response>> reader((stub->*func)(context, request));
            return std::move(reader);
        }
    }

    // 同步双流接口
    template<class Request, class Response, class GRPC_FUNC>
    std::unique_ptr<::grpc::ClientReaderWriter<Request, Response>> BSend(GRPC_FUNC func, ::grpc::ClientContext* context) {
        if (!stub_) {
            assert(false);
            return nullptr;
        }

        Stub* stub = stub_.get();

        if (!context) {
            ::grpc::ClientContext ctx;
            context = &ctx;
            std::unique_ptr<::grpc::ClientReaderWriter<Request, Response>> rw((stub->*func)(context));
            return std::move(rw);
        } else {
            std::unique_ptr<::grpc::ClientReaderWriter<Request, Response>> rw((stub->*func)(context));
            return std::move(rw);
        }
    }

};

// 异步客户端
template<typename T>
class AsyncClient : public BaseClient<T> {
public:
    typedef typename BaseClient<T>::Stub Stub;

protected:
    ::grpc::CompletionQueue* cq_ = nullptr;

public:
    AsyncClient() {
        cq_ = nullptr;
    }

    AsyncClient(::grpc::CompletionQueue* cq) {
        cq_ = cq;
    }

    ~AsyncClient() {}

    void SetQueue(::grpc::CompletionQueue* cq) {
        if (!cq_) {
            cq_ = cq;
        } else {
            throw "set queue twice";
        }
    }

    // 异步发送一元类接口
    template<class Request, class Response, class GRPC_FUNC, class ON_CALL>
    ::grpc::Status Send(GRPC_FUNC func, ON_CALL on_call, std::shared_ptr<::grpc::ClientContext> context, const Request& request) {
        if (!this->stub_) {
            assert(false);
            return ::grpc::Status::CANCELLED;
        }
        if (!context) {
            context = std::make_shared<::grpc::ClientContext>();
        }

        auto method = std::bind(func,
                                this->stub_.get(),
                                std::placeholders::_1,
                                std::placeholders::_2,
                                std::placeholders::_3);

        auto responder = method(context.get(), request, cq_);
        new ClientData<Request, Response>(context, std::move(responder), on_call);
        return ::grpc::Status::OK;
    }

    // 异步客户端流发送接口
    template<class Request, class Response, class GRPC_FUNC>
    std::shared_ptr<ClientStreamWriter<Request, Response>> CSend(GRPC_FUNC func, std::shared_ptr<::grpc::ClientContext> context) {
        if (!this->stub_) {
            assert(false);
            return nullptr;
        }
        if (!context) {
            context = std::make_shared<::grpc::ClientContext>();
        }

        auto method = std::bind(func,
                                this->stub_.get(),
                                std::placeholders::_1,
                                std::placeholders::_2,
                                std::placeholders::_3);

        return std::make_shared<ClientStreamWriter<Request, Response>>(cq_, context, method);
    }

    // 异步服务端流发送接口
    template<class Request, class Response, class GRPC_FUNC>
    std::shared_ptr<ClientStreamReader<Request, Response>> SSend(GRPC_FUNC func, std::shared_ptr<::grpc::ClientContext> context, const Request& request) {
        if (!this->stub_) {
            assert(false);
            return nullptr;
        }
        if (!context) {
            context = std::make_shared<::grpc::ClientContext>();
        }

        auto method = std::bind(func,
                                this->stub_.get(),
                                std::placeholders::_1,
                                std::placeholders::_2,
                                std::placeholders::_3);

        auto responder = method(context.get(), request, cq_);
        return std::make_shared<ClientStreamReader<Request, Response>>(context, std::move(responder));
    }

    // 异步双流接口
    template<class Request, class Response, class GRPC_FUNC>
    std::shared_ptr<ClientStreamReaderWriter<Request, Response>> BSend(GRPC_FUNC func, std::shared_ptr<::grpc::ClientContext> context) {
        if (!this->stub_) {
            assert(false);
            return nullptr;
        }
        if (!context) {
            context = std::make_shared<::grpc::ClientContext>();
        }

        auto method = std::bind(func,
                                this->stub_.get(),
                                std::placeholders::_1,
                                std::placeholders::_2);

        auto responder = method(context.get(), cq_);
        return std::make_shared<ClientStreamReaderWriter<Request, Response>>(context, std::move(responder));
    }
};

// 协程客户端
template<typename T>
class CoClient : public BaseClient<T> {
public:
    typedef typename BaseClient<T>::Stub Stub;
protected:
    ::grpc::CompletionQueue* cq_ = nullptr;

public:
    CoClient() {
        this->cq_ = DefCliBuilder()->GetQueue();
        DefCliBuilder()->StartLoop();
    }

    // 协程发送接口
    template<class Request, class Response, class GRPC_FUNC>
    ::grpc::Status
    Send(GRPC_FUNC func, std::shared_ptr<::grpc::ClientContext> context, const Request& request, Response* response) {
        if (!this->stub_) {
            assert(false);
            return ::grpc::Status::CANCELLED;
        }
        if (!context) {
            context = std::make_shared<::grpc::ClientContext>();
        }

        auto method = std::bind(func,
                                this->stub_.get(),
                                std::placeholders::_1,
                                std::placeholders::_2,
                                std::placeholders::_3);

        co_grpc::CoWaiter waiter;
        auto cb_status = std::make_shared<::grpc::Status>();
        auto cb_res = std::make_shared<Response>();

        auto responder = method(context.get(), request, cq_);
        // 回调不应比waiter.wait执行的还要快
        new ClientData<Request, Response>(context, std::move(responder), [waiter, cb_status, cb_res](::grpc::Status s, Response& r) {
            *cb_status = s;
            cb_res->Swap(&r);
            waiter.Resume();
        });

        waiter.wait(nullptr);
        response->Swap(cb_res.get());
        return *cb_status;
    }

    // 协程客户端流发送接口
    template<class Request, class Response, class GRPC_FUNC>
    std::shared_ptr<CoClientStreamWriter<Request, Response>>
    CSend(GRPC_FUNC func, std::shared_ptr<::grpc::ClientContext> context, Response* rsp) {
        if (!this->stub_) {
            assert(false);
            return nullptr;
        }
        if (!context) {
            context = std::make_shared<::grpc::ClientContext>();
        }

        auto method = std::bind(func,
                                this->stub_.get(),
                                std::placeholders::_1,
                                std::placeholders::_2,
                                std::placeholders::_3);

        auto writer = std::make_shared<CoClientStreamWriter<Request, Response>>(cq_, context, method, rsp);
        return writer;
    }

    // 协程服务端流发送接口
    template<class Request, class Response, class GRPC_FUNC>
    std::shared_ptr<CoClientStreamReader<Request, Response>> SSend(GRPC_FUNC func, std::shared_ptr<::grpc::ClientContext> context, const Request& request) {
        if (!this->stub_) {
            assert(false);
            return nullptr;
        }
        if (!context) {
            context = std::make_shared<::grpc::ClientContext>();
        }

        auto method = std::bind(func,
                                this->stub_.get(),
                                std::placeholders::_1,
                                std::placeholders::_2,
                                std::placeholders::_3);

        auto responder = method(context.get(), request, cq_);
        auto reader = std::make_shared<CoClientStreamReader<Request, Response>>(context, std::move(responder));
        return reader;
    }

    // 协程双流接口
    template<class Request, class Response, class GRPC_FUNC>
    std::shared_ptr<CoClientStreamReaderWriter<Request, Response>> BSend(GRPC_FUNC func, std::shared_ptr<::grpc::ClientContext> context) {
        if (!this->stub_) {
            assert(false);
            return nullptr;
        }
        if (!context) {
            context = std::make_shared<::grpc::ClientContext>();
        }

        auto method = std::bind(func,
                                this->stub_.get(),
                                std::placeholders::_1,
                                std::placeholders::_2);

        auto responder = method(context.get(), cq_);
        auto rw = std::make_shared<CoClientStreamReaderWriter<Request, Response>>(context, std::move(responder));
        typename CoClientStreamReaderWriter<Request, Response>::helper help;
        if (!help(rw.get())) {
            return nullptr;
        }
        return rw;
    }
};

// alias
template<typename T>
using Client = CoClient<T>;

}

