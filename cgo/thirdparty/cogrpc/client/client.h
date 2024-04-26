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
#include "channel.h"
#include "clientdata.h"
#include "client_builder.h"

namespace cogrpc {
    inline std::shared_ptr<::grpc::ClientContext> MakeContext(int timeout = 3000) {
        auto ctx = std::make_shared<grpc::ClientContext>();
        if (timeout > 0) {
            auto deadline = std::chrono::system_clock::now() + std::chrono::milliseconds(timeout);
            ctx->set_deadline(deadline);
        }
        return ctx;
    }

    // new a client context from server context
    inline std::shared_ptr<::grpc::ClientContext> FromServerContext(::grpc::ServerContext *ctx, ::grpc::PropagationOptions options = ::grpc::PropagationOptions()) {
        std::shared_ptr<grpc::ClientContext> newctx = std::move(::grpc::ClientContext::FromServerContext(*ctx, options));
        newctx->set_deadline(ctx->deadline());
        for (auto iter = ctx->client_metadata().begin(); iter != ctx->client_metadata().end(); iter++) {
            newctx->AddMetadata(std::string(iter->first.data(), iter->first.size()), std::string(iter->second.data(), iter->second.size()));
        }
        return newctx;
    }

    // 协程客户端.
    template<typename T>
    class CoClient {
    public:
        typedef typename T::Stub Stub;
    protected:
        ::grpc::CompletionQueue* cq_ = nullptr;
        std::shared_ptr<Stub> stub_;
        std::vector<std::unique_ptr<grpc::experimental::ClientInterceptorFactoryInterface>> interceptor_factories_;

    public:
        CoClient() {
            DefCliBuilder()->StartLoop();
            this->cq_ = DefCliBuilder()->GetQueue();
        }

        CoClient(const CoClient& b) {
            stub_ = b.stub_;
            cq_ = b.cq_;
        }

        CoClient& operator=(const CoClient& b) = delete;

        // 判断客户端是否有效.
        bool Valid() {
            return stub_ != 0;
        }

        // 绑定通道.
        void Bind(std::shared_ptr<::grpc::Channel> channel) {
            if (stub_) {
                return;
            }
            stub_ = std::move(T::NewStub(channel));
        }

        void Bind(const std::string& target, const std::string& lb_policy) {
            if (stub_) {
                return;
            }
            if (interceptor_factories_.empty()) {
                auto c = GetChannel(target, lb_policy);
                stub_ = std::move(T::NewStub(c));
            } else {
                auto c = GetChannelWithInterceptor(std::move(interceptor_factories_), target, lb_policy);
                stub_ = std::move(T::NewStub(c));
            }
        }

        template<class ClientInterceptorType>
        void AddInterceptor() {
            interceptor_factories_.push_back(std::move(CreateClientInterceptorFactory<ClientInterceptorType>()));
        }

    protected:
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

            typedef std::shared_ptr<::grpc::ClientAsyncResponseReader<Response>> Responder;
            Responder responder = std::move(method(context.get(), request, cq_));

            PointScoped ps(new ClientUnaryData<Request, Response, Responder>(responder));
            ps->doRequest();
            response->Swap(&ps->response_);
            return ps->status_;
        }

        // 协程客户端流发送接口.
        template<class Request, class Response, class GRPC_FUNC>
        GRPC_CLIENT_WRITER(Request, Response)
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

            auto response = new Response;
            typedef std::shared_ptr<::grpc::ClientAsyncWriter<Request>> Responder;
            Responder responder = std::move(method(context.get(), response, this->cq_));

            PointScoped start(new ClientCSStartData<Request, Response, Responder>(responder));
            start->doRequest();
            if (!start->ok_) {
                // 释放response
                delete response;
                return nullptr;
            }

            // response的生命令周期由writer来接管
            GRPC_CLIENT_WRITER(Request, Response) writer(new ClientStreamWriter<Request, Response, Responder>
                (responder, context, rsp, response));
            return writer;
        }

        // 协程服务端流发送接口.
        template<class Request, class Response, class GRPC_FUNC>
        GRPC_CLIENT_READER(Request, Response)
        SSend(GRPC_FUNC func, std::shared_ptr<::grpc::ClientContext> context, const Request& request) {
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

            typedef std::shared_ptr<::grpc::ClientAsyncReader<Response>> Responder;
            Responder responder = std::move(method(context.get(), request, cq_));

            PointScoped start(new ClientSSStartData<Request, Response, Responder>(responder));
            start->doRequest();
            if (!start->ok_) {
                return nullptr;
            }

            auto reader = std::make_shared<ClientStreamReader<Request, Response, Responder>>(responder, context);
            return reader;
        }

        // 协程双流接口.
        template<class Request, class Response, class GRPC_FUNC>
        GRPC_CLIENT_RW(Request, Response)
        BSend(GRPC_FUNC func, std::shared_ptr<::grpc::ClientContext> context) {
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

            typedef std::shared_ptr<::grpc::ClientAsyncReaderWriter<Request, Response>> Responder;
            Responder responder = std::move(method(context.get(), cq_));

            PointScoped start(new ClientDSStartData<Request, Response, Responder>(responder));
            start->doRequest();
            if (!start->ok_) {
                return nullptr;
            }

            auto rw = std::make_shared<ClientStreamReaderWriter<Request, Response, Responder>>(responder, context);
            return rw;
        }
    };

    // alias
    template<typename T>
    using Client = CoClient<T>;

}

