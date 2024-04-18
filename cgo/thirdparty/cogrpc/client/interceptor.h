//
// Created by xiaoqj on 2024/4/18.
//

#pragma once

#include <grpcpp/support/client_interceptor.h>

namespace cogrpc {
    class ClientInterceptor : public grpc::experimental::Interceptor {
    public:
        explicit ClientInterceptor(grpc::experimental::ClientRpcInfo* info) : rpcInfo_(info) {}

        // void Intercept(grpc::experimental::InterceptorBatchMethods* methods) override {}
    protected:
        grpc::experimental::ClientRpcInfo *rpcInfo_;
    };

    template<typename ClientInterceptorType>
    class ClientInterceptorFactory : public grpc::experimental::ClientInterceptorFactoryInterface {
    public:
        grpc::experimental::Interceptor *CreateClientInterceptor(
                grpc::experimental::ClientRpcInfo *info) override
        {
            return new ClientInterceptorType(info);
        }
    };

    template<typename ClientInterceptorType>
    std::unique_ptr<grpc::experimental::ClientInterceptorFactoryInterface>
    CreateClientInterceptorFactory() {
        return std::unique_ptr<grpc::experimental::ClientInterceptorFactoryInterface>
                (new ClientInterceptorFactory<ClientInterceptorType>);
    }
}
